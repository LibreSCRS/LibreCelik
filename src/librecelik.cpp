// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "config.h"
#include "document/eid/changepindlg.h"
#include "document/emrtd/emrtdauthwidget.h"
#include "document/tokensection.h"
#include "plugin/carddatautils.h"
#include "smartcard/smartcardreaderlistener.h"
#include "ui_librecelik.h"
#include "utils/libreceliklog.h"

#include <smartcard/pcsc_connection.h>

#include <algorithm>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpacerItem>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>

LibreCelik::LibreCelik(QWidget* parent) : QMainWindow(parent), ui(new Ui::LibreCelik)
{
    qCDebug(libreSCRSGeneral, "Setting up GUI");

    // Install translator BEFORE setupUi so the initial UI render is translated.
    // changeEvent is guarded by uiReady to avoid calling retranslateUi before
    // setupUi has run.
    QSettings settings("LibreSCRS", "LibreCelik");
    QString locale = settings.value("language", QString()).toString();

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
    ui->menubar->hide();

    // Auto-hide the status bar once its message is cleared (e.g. after showMessage timeout
    // or explicit clearMessage). This keeps the bar invisible except when in use.
    connect(ui->statusbar, &QStatusBar::messageChanged, this, [this](const QString& msg) {
        if (msg.isEmpty())
            ui->statusbar->hide();
    });

    // Build the language menu and set the button text to match the loaded locale.
    {
        auto* langMenu = new QMenu(ui->languageButton);
        auto* enAction = langMenu->addAction("English");
        auto* srAction = langMenu->addAction("Српски");
        ui->languageButton->setMenu(langMenu);
        connect(enAction, &QAction::triggered, this, [this]() { onLanguageChanged(0); });
        connect(srAction, &QAction::triggered, this, [this]() { onLanguageChanged(1); });
    }
    ui->languageButton->setText(locale.startsWith("sr") ? "Српски" : "English");

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
                            qtTrId("lc-main-about-libremiddleware").arg(LIBRECELIK_MIDDLEWARE_VERSION));
}

bool LibreCelik::loadLanguage(const QString& locale)
{
    if (locale.isEmpty())
        return false;
    QApplication::removeTranslator(&translator);
    if (translator.load(":/i18n/LibreCelik_" + locale)) {
        QApplication::installTranslator(&translator);
        this->locale = locale;
        return true;
    }
    return false;
}

void LibreCelik::onLanguageChanged(int index)
{
    QString locale = (index == 1) ? "sr_RS" : "en";
    QSettings settings("LibreSCRS", "LibreCelik");
    settings.setValue("language", locale);
    loadLanguage(locale);
    ui->languageButton->setText(index == 1 ? "Српски" : "English");
}

void LibreCelik::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange && uiReady) {
        ui->retranslateUi(this);
        updateAboutText();
        // retranslateUi resets the button to "English"; restore the actual locale.
        ui->languageButton->setText(locale.startsWith("sr") ? "Српски" : "English");
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
                                    auto* ts = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, self);
                                    ts->setHeaderColor(QColor(230, 135, 60));
                                    ts->setHeaderHeight(56);
                                    ts->setExpanded(!visible);
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
                        auto* ts = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, self);
                        ts->setHeaderColor(QColor(230, 135, 60));
                        ts->setHeaderHeight(56);
                        ts->setExpanded(!visible2);
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

    connect(asyncReader, &AsyncCardReader::errorOccurred, this, [this, reader](const QString& msg) {
        if (activeReaders.find(reader) == activeReaders.end())
            return; // reader was removed — stale queued signal
        ui->statusbar->show();
        ui->statusbar->showMessage(msg);
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
    if (activeReaders.empty())
        ui->stackedWidget->setCurrentIndex(0);
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
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
