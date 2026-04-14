// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "librecelik.h"
#include "config.h"
#include "settings/settingsdialog.h"
#include "settings/settingskeys.h"
#include "document/rs-eid/changepindlg.h"
#include "document/emrtd/emrtdauthwidget.h"
#include "document/tokensection.h"
#include "plugin/carddatautils.h"
#include "smartcard/smartcardreaderlistener.h"
#include "ui_librecelik.h"
#include "utils/libreceliklog.h"

#include <smartcard/pcsc_connection.h>

#ifdef LIBRECELIK_SIGNING_ENABLED
#include "signing/defaults.h"
#include "signing/signingwizard.h"
#include <libresign/signing_service_factory.h>
#endif

#ifdef Q_OS_MACOS
#include "utils/macos_menu.h"
#endif

#include <algorithm>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLibraryInfo>
#include <QLocale>
#include <QMenu>
#include <QMessageBox>
#include <QProgressBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpacerItem>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>

LibreCelik::LibreCelik(QWidget* parent) : QMainWindow(parent), ui(new Ui::LibreCelik)
{
    qCDebug(libreSCRSGeneral, "Setting up GUI");

    // Install translator BEFORE setupUi so the initial UI render is translated.
    // changeEvent is guarded by uiReady to avoid calling retranslateUi before
    // setupUi has run.
    QSettings settings(settings::kOrganization, settings::kApplication);
    QString locale = settings.value(settings::kLanguage, QString()).toString();

    if (!loadLanguage(locale)) {
        locale.clear();
        auto langs = QLocale::system().uiLanguages();
        for (const auto& l : std::as_const(langs)) {
            if (loadLanguage(QLocale(l).name())) {
                locale = QLocale(l).name();
                break;
            }
        }
        if (locale.isEmpty()) {
            loadLanguage("en");
            locale = "en";
        }
    }

    ui->setupUi(this);
    uiReady = true;

    // Load card plugins. In deployed packages (AppImage, DMG) the plugins live
    // next to the executable; fall back to the build-tree paths for development.
    auto resolvePluginDir = [](const QString& subdir, const char* buildFallback) -> QString {
        QDir appDir(QCoreApplication::applicationDirPath());
#ifdef Q_OS_MACOS
        // .app/Contents/MacOS/../PlugIns/<subdir>
        QDir bundleDir(appDir.filePath("../PlugIns/" + subdir));
#else
        // AppImage: usr/bin/../lib/<subdir>
        QDir bundleDir(appDir.filePath("../lib/" + subdir));
#endif
        if (bundleDir.exists())
            return bundleDir.absolutePath();
        return QString::fromUtf8(buildFallback);
    };

    middlewarePluginRegistry.loadPluginsFromDirectory(
        resolvePluginDir("middleware-plugins", LIBREMIDDLEWARE_PLUGIN_DIR).toStdString());
    guiPluginRegistry.loadPluginsFromDirectory(resolvePluginDir("gui-plugins", LIBRECELIK_GUI_PLUGIN_DIR));

    ui->stackedWidget->setCurrentIndex(0);

    connect(ui->readerComboBox, &QComboBox::currentIndexChanged, ui->readerStackedWidget,
            &QStackedWidget::setCurrentIndex);

    ui->statusbar->hide();
    // Menu bar
    editMenu = ui->menubar->addMenu(qtTrId("lc-menu-edit"));
    settingsAction = editMenu->addAction(qtTrId("lc-menu-settings"));
    settingsAction->setMenuRole(QAction::PreferencesRole);
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(settingsAction, &QAction::triggered, this, &LibreCelik::openSettings);

    helpMenu = ui->menubar->addMenu(qtTrId("lc-menu-help"));
    aboutAction = helpMenu->addAction(qtTrId("lc-menu-about"));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, &LibreCelik::showAboutDialog);
    aboutQtAction = helpMenu->addAction(qtTrId("lc-menu-about-qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
#ifdef Q_OS_MACOS
    macosRetranslateAppMenu(qtTrId("lc-menu-about"), qtTrId("lc-menu-settings"));
#endif

    // Auto-hide the status bar once its message is cleared (e.g. after showMessage timeout
    // or explicit clearMessage). This keeps the bar invisible except when in use.
    connect(ui->statusbar, &QStatusBar::messageChanged, this, [this](const QString& msg) {
        if (msg.isEmpty())
            ui->statusbar->hide();
    });

    updateAboutText();
    connect(&SmartCardReaderListener::instance(), &SmartCardReaderListener::smartCardReaderEventOccured, this,
            &LibreCelik::onCardEventReceived);
    connect(&SmartCardReaderListener::instance(), &SmartCardReaderListener::smartCardReaderEnumerationChanged, this,
            &LibreCelik::onSmartCardReaderEnumerationChanged);
}

void LibreCelik::updateAboutText()
{
    ui->aboutLabel->setText(QString("<br><br>") + qtTrId("lc-main-about-librecelik").arg(LIBRECELIK_VERSION) +
                            QString("<br>") +
                            qtTrId("lc-main-about-libremiddleware").arg(LIBRECELIK_MIDDLEWARE_VERSION) +
                            QString("<br>") + qtTrId("lc-main-about-donate"));
}

bool LibreCelik::loadLanguage(const QString& locale)
{
    if (locale.isEmpty())
        return false;
    QApplication::removeTranslator(&translator);
    QApplication::removeTranslator(&qtTranslator);
    if (translator.load(":/i18n/LibreCelik_" + locale)) {
        QApplication::installTranslator(&translator);
        // Load Qt's own translations (for About Qt dialog, standard buttons, etc.).
        // Missing translations for some locales are expected (e.g. sr_RS); the
        // application text is still available via our own .qm file installed
        // above, so a failed Qt-catalog load is not fatal.
        if (!qtTranslator.load("qt_" + locale, QLibraryInfo::path(QLibraryInfo::TranslationsPath))) {
            // Intentionally ignored — see comment above.
        }
        QApplication::installTranslator(&qtTranslator);
        this->locale = locale;
        return true;
    }
    return false;
}

void LibreCelik::retranslateMenuBar()
{
    editMenu->setTitle(qtTrId("lc-menu-edit"));
    settingsAction->setText(qtTrId("lc-menu-settings"));
    helpMenu->setTitle(qtTrId("lc-menu-help"));
    aboutAction->setText(qtTrId("lc-menu-about"));
    aboutQtAction->setText(qtTrId("lc-menu-about-qt"));
#ifdef Q_OS_MACOS
    macosRetranslateAppMenu(qtTrId("lc-menu-about"), qtTrId("lc-menu-settings"));
#endif
}

void LibreCelik::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange && uiReady) {
        ui->retranslateUi(this);
        retranslateMenuBar();
        updateAboutText();
    }
    QMainWindow::changeEvent(event);
}

void LibreCelik::onCardEventReceived(const smartcard::MonitorEvent& event)
{
    qCDebug(libreSCRSGeneral) << "MonitorEvent:"
                              << (event.type == smartcard::MonitorEvent::Type::CardInserted ? "CardInserted"
                                                                                            : "CardRemoved")
                              << "received on reader:" << QString::fromStdString(event.readerName);
    if (event.type == smartcard::MonitorEvent::Type::CardInserted) {
        // Invalidate any pending retries from a previous event on this reader
        // and create a fresh stop_source for this card insertion.
        readerStopSource[event.readerName].request_stop();
        readerStopSource[event.readerName] = {};
        addNewReader(event.readerName);
    } else if (event.type == smartcard::MonitorEvent::Type::CardRemoved) {
        // Cancel any pending retry timers for this reader.
        readerStopSource[event.readerName].request_stop();
        removeReader(event.readerName);
    }
}

void LibreCelik::onSmartCardReaderEnumerationChanged(const QStringList& scrNames)
{
    std::vector<std::string> readers;
    for (const auto& [name, _] : activeReaders)
        readers.push_back(name);

    std::vector<std::string> scrNamesStd;
    scrNamesStd.reserve(static_cast<size_t>(scrNames.size()));
    for (const auto& s : scrNames)
        scrNamesStd.push_back(s.toStdString());
    std::sort(scrNamesStd.begin(), scrNamesStd.end());

    // Remove unplugged readers
    std::vector<std::string> toRemove;
    std::set_difference(std::begin(readers), std::end(readers), std::begin(scrNamesStd), std::end(scrNamesStd),
                        std::inserter(toRemove, std::begin(toRemove)));
    for (const auto& scrName : toRemove) {
        removeReader(scrName);
    }
}

void LibreCelik::addNewReader(std::string reader, int retryCount)
{
    qCDebug(libreSCRSGeneral) << "addNewReader:" << QString::fromStdString(reader) << "retry=" << retryCount
                              << "activeReaders.count=" << activeReaders.count(reader);

    if (retryCount == 0) {
        // Fresh card event: defensively remove any stale widget left over from a
        // fast swap where CardRemoved wasn't emitted (no-op if nothing registered).
        removeReader(reader);
    } else if (activeReaders.count(reader)) {
        // Retry timer: a widget was created while this timer was pending — stop.
        return;
    }

    auto stopToken = readerStopSource[reader].get_token();

    std::unique_ptr<smartcard::PCSCConnection> conn;
    std::vector<uint8_t> atr;
    try {
        conn = std::make_unique<smartcard::PCSCConnection>(reader);
        atr = conn->getATR();
    } catch (const std::exception&) {
        if (retryCount < 2) {
            QTimer::singleShot(300, this, [this, reader, retryCount, stopToken]() {
                if (stopToken.stop_requested())
                    return;
                addNewReader(reader, retryCount + 1);
            });
        } else {
            ui->statusbar->show();
            ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
        }
        return;
    }

    auto candidates = middlewarePluginRegistry.findAllCandidates(atr, *conn);
    if (candidates.empty()) {
        if (retryCount < 2) {
            QTimer::singleShot(300, this, [this, reader, retryCount, stopToken]() {
                if (stopToken.stop_requested())
                    return;
                addNewReader(reader, retryCount + 1);
            });
        } else {
            ui->statusbar->show();
            ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
        }
        return;
    }

    // A valid card is being added — clear any previous unsupported-card notice.
    ui->statusbar->clearMessage();

    auto* asyncReader = new AsyncCardReader(std::move(candidates),
                                            std::vector<plugin::CardPlugin*>(middlewarePluginRegistry.plugins().begin(),
                                                                             middlewarePluginRegistry.plugins().end()),
                                            std::move(conn), this);

    // Show loading spinner immediately
    auto* spinnerWidget = new QWidget(this);
    spinnerWidget->setProperty("isSpinner", true);
    {
        auto* layout = new QVBoxLayout(spinnerWidget);
        layout->setAlignment(Qt::AlignCenter);
        auto* bar = new QProgressBar(spinnerWidget);
        bar->setRange(0, 0);
        bar->setFixedWidth(200);
        bar->setTextVisible(false);
        auto* label = new QLabel(qtTrId("lc-reading-card"), spinnerWidget);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(bar);
        layout->addWidget(label);
    }

    int spinnerIdx = ui->readerStackedWidget->addWidget(spinnerWidget);
    ui->readerComboBox->addItem(QString::fromStdString(reader));
    ui->readerComboBox->setCurrentIndex(spinnerIdx);
    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);
    activeReaders[reader] = {asyncReader, spinnerWidget};
    ui->stackedWidget->setCurrentIndex(1);

    // Helper: detect if current widget is the loading spinner
    auto isSpinner = [](QWidget* w) { return w && w->property("isSpinner").toBool(); };

    // Helper: replace the current widget for a reader with a new one
    auto replaceWidget = [this, reader](QWidget* newWidget) {
        auto it = activeReaders.find(reader);
        if (it == activeReaders.end())
            return;
        auto* oldWidget = it->second.widget;
        int widx = ui->readerStackedWidget->indexOf(oldWidget);
        ui->readerStackedWidget->removeWidget(oldWidget);
        oldWidget->deleteLater();
        ui->readerStackedWidget->insertWidget(widx, newWidget);
        ui->readerStackedWidget->setCurrentIndex(widx);
        it->second.widget = newWidget;
    };

    connect(asyncReader, &AsyncCardReader::cardDataReady, this,
            [this, asyncReader, reader, isSpinner, replaceWidget](const plugin::CardData& data) {
                auto it = activeReaders.find(reader);
                if (it == activeReaders.end())
                    return;
                QWidget* self = this;

                bool streamedWidget = it->second.widget && !isSpinner(it->second.widget);

                // Phase 2 re-reads are handled by the dialog's own lambda.
                if (streamedWidget && data.findGroup("auth_required"))
                    return;

                auto* guiPlugin = guiPluginRegistry.findByCardType(QString::fromStdString(data.cardType));
                if (!guiPlugin) {
                    ui->statusbar->show();
                    ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
                    return;
                }

                // eMRTD two-phase auth: show inline CAN widget in the reader tab.
                // The auth widget sets the "isSpinner" property so isSpinner() returns true.
                // When PACE succeeds, cardGroupReady detects this and replaces the auth
                // widget with the streaming card data widget — no explicit success handler needed.
                if (data.findGroup("auth_required")) {
                    // Re-entry guard: on wrong CAN retry, requestDataWithCredentials emits
                    // cardDataReady with auth_required + error groups. Route the error to
                    // the existing widget instead of creating a duplicate.
                    if (auto* existing = qobject_cast<EMRTDAuthWidget*>(it->second.widget)) {
                        if (data.findGroup("error")) {
                            auto errMsg = plugin::getFieldValue(data.findGroup("error"), "error");
                            existing->onAuthFailed(errMsg.isEmpty() ? qtTrId("lc-error-auth-failed") : errMsg);
                        }
                        return;
                    }

                    auto* authWidget = new EMRTDAuthWidget(this);
                    auto paceFlag = plugin::getFieldValue(data.findGroup("auth_required"), "pace_supported");
                    authWidget->setDefaultTab(paceFlag == "true");

                    connect(authWidget, &EMRTDAuthWidget::credentialsEntered, asyncReader,
                            &AsyncCardReader::requestDataWithCredentials);

                    connect(asyncReader, &AsyncCardReader::errorOccurred, authWidget, &EMRTDAuthWidget::onAuthFailed);

                    replaceWidget(authWidget);
                    return;
                }

                auto hasVisibleData = [&data]() {
                    return std::any_of(data.groups.begin(), data.groups.end(), [](const auto& g) {
                        return g.groupKey != "auth_required" && g.groupKey != "error" && g.groupKey != "presence" &&
                               g.groupKey != "token" && g.groupKey != "meta" && g.groupKey != "certificates" &&
                               g.groupKey != "pins" && !g.fields.empty();
                    });
                };

                // Streaming already built the card widget — just append TokenSection
                if (streamedWidget) {
                    bool visible = hasVisibleData();

                    // Show auth failure message if card section is empty
                    if (!visible) {
                        auto* scrollArea = qobject_cast<QScrollArea*>(it->second.widget);
                        if (scrollArea && scrollArea->widget()) {
                            auto children =
                                scrollArea->widget()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                            if (!children.isEmpty())
                                guiPlugin->showNoDataMessage(children.first());
                        }
                    }

                    // Enable print button now that all data has arrived
                    if (visible && guiPlugin->supportsPrinting()) {
                        auto* scrollArea = qobject_cast<QScrollArea*>(it->second.widget);
                        if (scrollArea && scrollArea->widget()) {
                            auto* containerLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
                            if (containerLayout && containerLayout->count() > 0) {
                                auto* cardWidget = containerLayout->itemAt(0)->widget();
                                guiPlugin->enablePrintButton(cardWidget);
                            }
                        }
                    }
                    if (asyncReader->hasPKI()) {
                        auto* scrollArea = qobject_cast<QScrollArea*>(it->second.widget);
                        if (scrollArea && scrollArea->widget()) {
                            auto* containerLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
                            if (containerLayout) {
                                auto* pkiWidget = guiPlugin->createPKIWidget(self);
                                if (!pkiWidget) {
                                    auto* ts = new TokenSection({LIBRECELIK_CERTIFICATES_DIR}, self);
                                    ts->setHeaderColor(QColor(230, 135, 60));
                                    ts->setHeaderHeight(56);
                                    ts->setExpanded(!visible);
#ifdef LIBRECELIK_SIGNING_ENABLED
                                    ts->setReaderName(reader);
#endif
                                    pkiWidget = ts;
                                }
                                connectPKISignals(asyncReader, pkiWidget);
                                containerLayout->addWidget(pkiWidget);
                                asyncReader->requestCertificates();
                            }
                        }
                    }
                    return;
                }

                QWidget* topWidget = guiPlugin->createWidget(data, self);
                bool visible2 = hasVisibleData();

                if (!visible2)
                    guiPlugin->showNoDataMessage(topWidget);

                // Enable print button for the non-streaming (full-data) path
                if (visible2 && guiPlugin->supportsPrinting())
                    guiPlugin->enablePrintButton(topWidget);

                if (asyncReader->hasPKI()) {
                    auto* pkiWidget = guiPlugin->createPKIWidget(self);
                    if (!pkiWidget) {
                        auto* ts = new TokenSection({LIBRECELIK_CERTIFICATES_DIR}, self);
                        ts->setHeaderColor(QColor(230, 135, 60));
                        ts->setHeaderHeight(56);
                        ts->setExpanded(!visible2);
#ifdef LIBRECELIK_SIGNING_ENABLED
                        ts->setReaderName(reader);
#endif
                        pkiWidget = ts;
                    }
                    connectPKISignals(asyncReader, pkiWidget);

                    auto* container = new QWidget(this);
                    auto* containerLayout = new QVBoxLayout(container);
                    containerLayout->setContentsMargins(0, 0, 0, 0);
                    containerLayout->setAlignment(Qt::AlignTop);
                    containerLayout->addWidget(topWidget);
                    containerLayout->addWidget(pkiWidget);

                    auto* scrollArea = new QScrollArea(this);
                    scrollArea->setWidgetResizable(true);
                    scrollArea->setFrameShape(QFrame::NoFrame);
                    scrollArea->setWidget(container);
                    topWidget = scrollArea;

                    asyncReader->requestCertificates();
                } else {
                    auto* container = new QWidget(this);
                    auto* containerLayout = new QVBoxLayout(container);
                    containerLayout->setContentsMargins(0, 0, 0, 0);
                    containerLayout->setAlignment(Qt::AlignTop);
                    containerLayout->addWidget(topWidget);

                    auto* scrollArea = new QScrollArea(this);
                    scrollArea->setWidgetResizable(true);
                    scrollArea->setFrameShape(QFrame::NoFrame);
                    scrollArea->setWidget(container);
                    topWidget = scrollArea;
                }

                replaceWidget(topWidget);
            });

    connect(asyncReader, &AsyncCardReader::errorOccurred, this, [this, reader, isSpinner](const QString& msg) {
        auto it = activeReaders.find(reader);
        if (it == activeReaders.end())
            return; // reader was removed — stale queued signal
        ui->statusbar->show();
        ui->statusbar->showMessage(msg);
        // If we're still showing the spinner, all candidates failed during
        // initial read — remove the stuck reader entry so the UI resets.
        if (isSpinner(it->second.widget))
            removeReader(reader);
    });

    // Progressive display: replace spinner with empty widget on first group, then add groups
    connect(asyncReader, &AsyncCardReader::cardGroupReady, this,
            [this, reader, isSpinner, replaceWidget](const QString& cardType, const plugin::CardFieldGroup& group) {
                if (group.groupKey == "auth_required" || group.groupKey == "error")
                    return;

                auto it = activeReaders.find(reader);
                if (it == activeReaders.end())
                    return;

                auto* guiPlugin = guiPluginRegistry.findByCardType(cardType);
                if (!guiPlugin)
                    return;

                auto* currentWidget = it->second.widget;

                // First group: replace spinner with empty widget shell
                if (isSpinner(currentWidget)) {
                    auto* emptyWidget = guiPlugin->createEmptyWidget(this);
                    if (!emptyWidget)
                        return; // plugin doesn't support streaming — wait for cardDataReady

                    auto* container = new QWidget(this);
                    auto* containerLayout = new QVBoxLayout(container);
                    containerLayout->setContentsMargins(0, 0, 0, 0);
                    containerLayout->setAlignment(Qt::AlignTop);
                    containerLayout->addWidget(emptyWidget);

                    auto* scrollArea = new QScrollArea(this);
                    scrollArea->setWidgetResizable(true);
                    scrollArea->setFrameShape(QFrame::NoFrame);
                    scrollArea->setWidget(container);

                    replaceWidget(scrollArea);
                }

                // Find the plugin widget inside the scroll area and add the group
                auto* scrollArea = qobject_cast<QScrollArea*>(it->second.widget);
                if (!scrollArea || !scrollArea->widget())
                    return;

                auto children = scrollArea->widget()->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
                if (!children.isEmpty())
                    guiPlugin->addGroup(group, children.first());
            });

    asyncReader->requestData();
}

void LibreCelik::removeReader(std::string reader)
{
    auto it = activeReaders.find(reader);
    if (it == activeReaders.end())
        return;

    auto* asyncReader = it->second.reader;
    auto* widget = it->second.widget;

    if (widget) {
        int idx = ui->readerStackedWidget->indexOf(widget);
        ui->readerComboBox->removeItem(idx);
        ui->readerStackedWidget->removeWidget(widget);
        widget->deleteLater();
    }

    // Disconnect all signals immediately to prevent stale callbacks
    asyncReader->disconnect();
    activeReaders.erase(it);
    readerStopSource.erase(reader);

    // Signal workers to stop (non-blocking)
    asyncReader->initiateCancel();

    // Complete cleanup in a background thread so the GUI never blocks
    // waiting for a PC/SC timeout (SCardCancel does not cancel SCardTransmit
    // on Linux/pcsclite).
    auto* cleanupThread = QThread::create([asyncReader]() {
        asyncReader->waitForPendingAsync();
        try {
            asyncReader->clearPluginCredentials();
        } catch (...) {
            // Card already removed — credentials are moot
        }
    });
    connect(cleanupThread, &QThread::finished, asyncReader, &QObject::deleteLater);
    connect(cleanupThread, &QThread::finished, cleanupThread, &QObject::deleteLater);
    cleanupThread->start();

    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);
    if (activeReaders.empty()) {
        ui->stackedWidget->setCurrentIndex(0);
        ui->statusbar->clearMessage();
    }
}

void LibreCelik::connectPKISignals(AsyncCardReader* reader, QWidget* pkiWidget)
{
    auto* tokenSection = qobject_cast<TokenSection*>(pkiWidget);
    if (!tokenSection)
        return;

    connect(reader, &AsyncCardReader::tokenInfoReady, tokenSection, &TokenSection::setTokenInfo);
    connect(reader, &AsyncCardReader::certificatesReady, tokenSection, &TokenSection::setCertificates);
    connect(reader, &AsyncCardReader::certificatesReady, reader, &AsyncCardReader::requestPINList);
    connect(reader, &AsyncCardReader::pinListReady, tokenSection, &TokenSection::setPINList);
    connect(tokenSection, &TokenSection::changePINRequested, this,
            [this, reader](uint8_t pinRef, const QString& pinLabel, bool isTransport, int minLength, int maxLength) {
                QWidget* self = this;
                auto dlg = std::make_unique<ChangePinDlg>(pinLabel, isTransport, minLength, maxLength, self);
                connect(dlg.get(), &ChangePinDlg::pinChangeRequested, reader,
                        [reader, pinRef](const QString& oldPin, const QString& newPin) {
                            reader->requestChangePIN(pinRef, oldPin, newPin);
                        });
                connect(reader, &AsyncCardReader::pinStatusReady, dlg.get(), &ChangePinDlg::onPinTriesLeftRead);
                connect(reader, &AsyncCardReader::pinChangeResult, dlg.get(),
                        [dlg = dlg.get()](bool success, int triesLeft, const QString& errorMessage) {
                            if (success)
                                dlg->onPinChangeSuccess();
                            else
                                dlg->onPinChangeFailed(triesLeft, triesLeft == 0, errorMessage);
                        });
                reader->requestPINTriesLeft(pinRef);
                dlg->exec();
            });

#ifdef LIBRECELIK_SIGNING_ENABLED
    connect(tokenSection, &TokenSection::signRequested, this,
            [this](const plugin::CertificateData& cert, const std::string& readerName) {
                if (!signingService)
                    signingService = libresign::createSigningService(libresign::Backend::DSS);

                if (!signingService)
                    return;

                // Build trust configuration from settings
                QSettings settings(settings::kOrganization, settings::kApplication);
                libresign::TrustConfig trustConfig;

                auto tlEntries = QJsonDocument::fromJson(settings.value(settings::kTslEntries).toByteArray()).array();
                if (tlEntries.isEmpty())
                    tlEntries = settings.value(settings::kTslEntries).toJsonArray();
                if (tlEntries.isEmpty()) {
                    for (const auto& d : signing::defaultTrustedLists())
                        trustConfig.trustedLists.push_back({d.url.toStdString(), d.lotl, d.eager});
                } else {
                    for (const auto& entry : tlEntries) {
                        auto obj = entry.toObject();
                        trustConfig.trustedLists.push_back({
                            obj["url"].toString().toStdString(),
                            obj["lotl"].toBool(false),
                            obj["eager"].toBool(true),
                        });
                    }
                }

                QString cacheDir = settings.value(settings::kTslCacheDir).toString();
                if (cacheDir.isEmpty()) {
                    QString xdgCache = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
                    cacheDir = xdgCache + "/librescrs/tsl";
                }
                trustConfig.cacheDirectory = cacheDir.toStdString();

                SigningWizard wizard(cert, readerName, signingService.get(), this);
                wizard.setTrustConfig(trustConfig);
                wizard.exec();
            });
#endif
}

void LibreCelik::openSettings()
{
    SettingsDialog dlg(this);
    connect(&dlg, &SettingsDialog::languageChanged, this,
            [this](const QString& newLocale) { loadLanguage(newLocale); });
    dlg.exec();
}

void LibreCelik::showAboutDialog()
{
    QString text = QStringLiteral("<p>") + qtTrId("lc-main-about-librecelik").arg(LIBRECELIK_VERSION) +
                   QStringLiteral("</p><p>") +
                   qtTrId("lc-main-about-libremiddleware").arg(LIBRECELIK_MIDDLEWARE_VERSION) + QStringLiteral("</p>");
    QMessageBox::about(this, qtTrId("lc-about-title"), text);
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
