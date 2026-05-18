// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "librecelik.h"
#include "aboutdialog.h"
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

#include <LibreSCRS/Plugin/CardPluginService.h>
#include <LibreSCRS/SmartCard/CardSession.h>

#ifdef LIBRECELIK_SIGNING_ENABLED
#include "signing/signingconfiguration.h"
#include "signing/signingwizard.h"
#include <LibreSCRS/Signing/SigningService.h>
#include <LibreSCRS/Trust/TrustStoreService.h>
#endif

#ifdef Q_OS_MACOS
#include "utils/macos_menu.h"
#endif

#include "utils/localeresolver.h"

#include <algorithm>
#include <filesystem>
#include <memory>

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QEvent>
#include <QLabel>
#include <QLibraryInfo>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpacerItem>
#include <QStandardPaths>
#include <QTimer>
#include <QThread>
#include <QVBoxLayout>

namespace {

/// Format ATR (or any byte sequence) as a hex QString.
///
/// `sep == '\0'`        flat, no separator: "3bf99600"
/// `sep == ' '`         display:            "3B F9 96 00"
/// `sep == ':'`         canonical:          "3B:F9:96:00"
/// `upper`              true → uppercase hex digits (A-F)
/// `maxBytes`           0 → format all; >0 → truncate to first N bytes
///                      and append " ..." when the source is longer
///                      (truncation marker is space-prefixed regardless
///                      of `sep`, so log + display forms both read clean).
QString atrToHex(const std::vector<std::uint8_t>& bytes, char sep = '\0', bool upper = false, int maxBytes = 0)
{
    const char* fmt = upper ? "%02X" : "%02x";
    const int total = static_cast<int>(bytes.size());
    const int n = (maxBytes > 0 && maxBytes < total) ? maxBytes : total;
    const bool truncated = n < total;

    qsizetype cap = static_cast<qsizetype>(n) * 2;
    if (sep != '\0' && n > 1)
        cap += n - 1;
    if (truncated)
        cap += 4;

    QString out;
    out.reserve(cap);
    for (int i = 0; i < n; ++i) {
        if (i > 0 && sep != '\0')
            out.append(QLatin1Char(sep));
        out.append(QString::asprintf(fmt, bytes[i]));
    }
    if (truncated)
        out.append(QStringLiteral(" ..."));
    return out;
}

} // namespace

LibreCelik::LibreCelik(QWidget* parent) : QMainWindow(parent), ui(new Ui::LibreCelik)
{
    qCDebug(lcGeneral, "Setting up GUI");

    // Install translator BEFORE setupUi so the initial UI render is translated.
    // changeEvent is guarded by uiReady to avoid calling retranslateUi before
    // setupUi has run.
    QSettings settings(settings::kOrganization, settings::kApplication);
    const QString resolved = utils::resolveActiveLocale(settings.value(settings::kLanguage, QString()).toString(),
                                                        utils::supportedLocaleCodes(), QLocale::system().uiLanguages());
    loadLanguage(resolved);

    ui->setupUi(this);
    uiReady = true;

#ifdef LIBRECELIK_SIGNING_ENABLED
    // Construct the shared LibreSCRS::Signing::SigningService once per
    // application lifetime via pure constructor DI (4.0 contract; no
    // instance()/configure*() accessors). Trust and TSA policy are loaded
    // from QSettings via SigningConfiguration and passed at construction
    // time; config is immutable post-ctor. The signing wizard (constructed
    // below per request) consumes the shared_ptr via injection.
    signingConfiguration = std::make_unique<signing::SigningConfiguration>();
    // TrustStoreService is the lifecycle owner of the
    // unified TrustStore. create() is non-throwing and never blocks on
    // network IO; eager TL fetches run on internal worker threads and
    // merge into the public store as they complete. Cert viewer and other
    // consumers obtain the store via trustService->trustStore().
    {
        auto trustResult = LibreSCRS::Trust::TrustStoreService::create(signingConfiguration->makeTrustConfig());
        if (!trustResult) {
            qCWarning(lcGeneral) << "Trust store init failed:"
                                 << QString::fromStdString(trustResult.error().userMessage.defaultText);
            // Fall back to a default/empty TrustStoreService so the rest of the
            // GUI lifecycle doesn't break — best-effort match for the existing
            // 'never null' assumption downstream. If the fallback also fails
            // (only realistic under catastrophic OOM), surface critically and
            // leave trustService null; downstream signing paths will see the
            // null and fail with clear diagnostics rather than silently no-op.
            LibreSCRS::Trust::TrustConfig fallback;
            fallback.includeSystemTrustStore = false;
            auto fallbackResult = LibreSCRS::Trust::TrustStoreService::create(std::move(fallback));
            if (fallbackResult) {
                trustService = *fallbackResult;
            } else {
                qCCritical(lcGeneral) << "Trust store fallback init ALSO failed:"
                                      << QString::fromStdString(fallbackResult.error().userMessage.defaultText)
                                      << "— signing flows will fail until the host is restarted.";
                trustService = nullptr;
            }
        } else {
            trustService = *trustResult;
        }
    }
    // SigningConfiguration::makeTsaProvider returns a dynamic provider; the
    // std::function is always non-empty post commit 2233ddb (review D2).
    // sign() surfaces TsaUnreachable if the resolved URL is empty or the
    // TSA is offline.
    signingService =
        std::make_shared<LibreSCRS::Signing::SigningService>(trustService, signingConfiguration->makeTsaProvider());
#endif

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

    // LM 4.0: CardPluginService is ctor-loaded and move-only. Pass the
    // middleware-plugins path at construction; the registry's LoadReport
    // captures per-plugin load outcomes if diagnostics are needed.
    // Trust subsystem: also pass the unified TrustStore obtained from SigningService;
    // the registry calls CardPlugin::setTrustStore on every loaded plugin
    // before exposing them, so plugins (rs-eid for card-data verification)
    // get trust anchors from a single source.
    middlewarePluginRegistry = std::make_shared<LibreSCRS::Plugin::CardPluginService>(
        std::filesystem::path{resolvePluginDir("middleware-plugins", LIBREMIDDLEWARE_PLUGIN_DIR).toStdString()},
        trustService ? trustService->trustStore() : nullptr);
    guiPluginRegistry.loadPluginsFromDirectory(resolvePluginDir("gui-plugins", LIBRECELIK_GUI_PLUGIN_DIR));

    ui->stackedWidget->setCurrentIndex(0);

    connect(ui->readerComboBox, &QComboBox::currentIndexChanged, ui->readerStackedWidget,
            &QStackedWidget::setCurrentIndex);

    ui->statusbar->hide();
    // Menu bar. On macOS the Edit menu is omitted entirely — LC has no edit
    // actions of its own, and QAction::PreferencesRole moves Settings into the
    // application menu, so the Edit menu would otherwise be left empty (filled
    // only with macOS-auto-injected Dictation/Emoji entries). Settings is then
    // parented to helpMenu purely so the role-based promotion has an anchor;
    // it is moved out to the application menu at runtime.
#ifndef Q_OS_MACOS
    editMenu = ui->menubar->addMenu(qtTrId("lc-menu-edit"));
    settingsAction = editMenu->addAction(qtTrId("lc-menu-settings"));
#endif
    helpMenu = ui->menubar->addMenu(qtTrId("lc-menu-help"));
#ifdef Q_OS_MACOS
    settingsAction = helpMenu->addAction(qtTrId("lc-menu-settings"));
#endif
    settingsAction->setMenuRole(QAction::PreferencesRole);
    settingsAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Comma));
    connect(settingsAction, &QAction::triggered, this, &LibreCelik::openSettings);

    aboutAction = helpMenu->addAction(qtTrId("lc-menu-about"));
    aboutAction->setMenuRole(QAction::AboutRole);
    connect(aboutAction, &QAction::triggered, this, &LibreCelik::showAboutDialog);
    aboutQtAction = helpMenu->addAction(qtTrId("lc-menu-about-qt"));
    aboutQtAction->setMenuRole(QAction::AboutQtRole);
    connect(aboutQtAction, &QAction::triggered, qApp, &QApplication::aboutQt);
#ifdef Q_OS_MACOS
    macosRetranslateAppMenu({.about = qtTrId("lc-menu-about"),
                             .preferences = qtTrId("lc-menu-settings"),
                             .services = qtTrId("lc-menu-services"),
                             .hide = qtTrId("lc-menu-hide"),
                             .hideOthers = qtTrId("lc-menu-hide-others"),
                             .showAll = qtTrId("lc-menu-show-all"),
                             .quit = qtTrId("lc-menu-quit")});
#endif

    // Auto-hide the status bar once its message is cleared (e.g. after showMessage timeout
    // or explicit clearMessage). This keeps the bar invisible except when in use.
    connect(ui->statusbar, &QStatusBar::messageChanged, this, [this](const QString& msg) {
        if (msg.isEmpty())
            ui->statusbar->hide();
    });

    updateAboutText();
    connect(smartCardReaderListener(), &SmartCardReaderListener::smartCardReaderEventOccured, this,
            &LibreCelik::onCardEventReceived);
    connect(smartCardReaderListener(), &SmartCardReaderListener::smartCardReaderEnumerationChanged, this,
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
    if (editMenu)
        editMenu->setTitle(qtTrId("lc-menu-edit"));
    settingsAction->setText(qtTrId("lc-menu-settings"));
    helpMenu->setTitle(qtTrId("lc-menu-help"));
    aboutAction->setText(qtTrId("lc-menu-about"));
    aboutQtAction->setText(qtTrId("lc-menu-about-qt"));
#ifdef Q_OS_MACOS
    macosRetranslateAppMenu({.about = qtTrId("lc-menu-about"),
                             .preferences = qtTrId("lc-menu-settings"),
                             .services = qtTrId("lc-menu-services"),
                             .hide = qtTrId("lc-menu-hide"),
                             .hideOthers = qtTrId("lc-menu-hide-others"),
                             .showAll = qtTrId("lc-menu-show-all"),
                             .quit = qtTrId("lc-menu-quit")});
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

void LibreCelik::onCardEventReceived(const LibreSCRS::SmartCard::MonitorEvent& event)
{
    using Kind = LibreSCRS::SmartCard::MonitorEvent::Kind;
    qCDebug(lcGeneral) << "MonitorEvent:" << (event.kind == Kind::CardInserted ? "CardInserted" : "CardRemoved")
                       << "received on reader:" << QString::fromStdString(event.readerName);
    qCDebug(lcProbeQuieting) << "LC-EVENT kind=" << (event.kind == Kind::CardInserted ? "CardInserted" : "CardRemoved")
                             << "reader=" << QString::fromStdString(event.readerName)
                             << "activeReaders=" << activeReaders.size();
    if (event.kind == Kind::CardInserted) {
        // Invalidate any pending retries from a previous event on this reader
        // and create a fresh stop_source for this card insertion.
        readerStopSource[event.readerName].request_stop();
        readerStopSource[event.readerName] = {};
        addNewReader(event.readerName);
    } else if (event.kind == Kind::CardRemoved) {
        // Announce removal BEFORE tearing down the reader's bookkeeping so
        // that any in-flight UI flow tied to this reader (the modal
        // SigningWizard is the canonical one) can act while its session
        // and plugin handles are still valid. The synchronous
        // Qt::AutoConnection path means subscribers run on the GUI
        // thread, in line, ahead of removeReader.
        emit cardRemoved(QString::fromStdString(event.readerName));
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
    qCDebug(lcGeneral) << "addNewReader:" << QString::fromStdString(reader) << "retry=" << retryCount
                       << "activeReaders.count=" << activeReaders.count(reader);
    qCDebug(lcProbeQuieting) << "LC-ADD-ENTRY reader=" << QString::fromStdString(reader) << "retry=" << retryCount
                             << "activeReaders.has=" << (activeReaders.count(reader) > 0)
                             << "caller=" << (retryCount == 0 ? "event" : "timer");

    if (retryCount == 0) {
        // Fresh card event: defensively remove any stale widget left over from a
        // fast swap where CardRemoved wasn't emitted (no-op if nothing registered).
        removeReader(reader);
        // Give previous AsyncCardReader's PCSC handle time to release before connecting.
        // macOS PCSC daemon serializes SCardTransmit per reader, so a lingering
        // handle from the cleanup thread would block our APDU probing.
        auto stopToken = readerStopSource[reader].get_token();
        QTimer::singleShot(200, this, [this, reader, stopToken]() {
            if (stopToken.stop_requested())
                return;
            addNewReader(reader, 1);
        });
        return;
    } else if (activeReaders.count(reader)) {
        // Retry timer: a widget was created while this timer was pending — stop.
        return;
    }

    auto stopToken = readerStopSource[reader].get_token();

    std::shared_ptr<LibreSCRS::SmartCard::CardSession> session;
    {
        auto opened = LibreSCRS::SmartCard::CardSession::open(reader);
        if (!opened.has_value()) {
            if (retryCount < 6) {
                int delay = 200 + retryCount * 150; // progressive: 350, 500, 650, 800, 950ms
                qCDebug(lcProbeQuieting) << "LC-PROBE-RETRY reader=" << QString::fromStdString(reader)
                                         << "retry=" << retryCount << "delay=" << delay;
                QTimer::singleShot(delay, this, [this, reader, retryCount, stopToken]() {
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
        session = std::make_shared<LibreSCRS::SmartCard::CardSession>(std::move(*opened));
    }

    qCDebug(lcProbeQuieting) << "LC-SESSION-OPEN reader=" << QString::fromStdString(reader) << "retry=" << retryCount
                             << "atr=" << atrToHex(session->atr());
    qCDebug(lcProbeQuieting) << "LC-PROBE-CALL reader=" << QString::fromStdString(reader) << "retry=" << retryCount
                             << "atr=" << atrToHex(session->atr());
    auto candidates = middlewarePluginRegistry->findAllCandidates(session->atr(), *session);
    if (candidates.empty()) {
        // Session opened successfully but no plugin matched — card is
        // definitively unsupported. The middleware plugin chain has had full
        // APDU access to the card; retrying changes nothing.
        QString atrSnippet = atrToHex(session->atr(), ' ', /*upper=*/true, /*maxBytes=*/6);
        ui->statusbar->show();
        ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card-with-atr").arg(atrSnippet));
        return;
    }

    // A valid card is being added — clear any previous unsupported-card notice.
    ui->statusbar->clearMessage();

    auto* asyncReader =
        new AsyncCardReader(std::move(candidates), middlewarePluginRegistry->plugins(), std::move(session), this);

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
            [this, asyncReader, reader, isSpinner, replaceWidget](const LibreSCRS::Plugin::CardData& data) {
                auto it = activeReaders.find(reader);
                if (it == activeReaders.end())
                    return;
                QWidget* self = this;

                bool streamedWidget = it->second.widget && !isSpinner(it->second.widget);

                // Authenticated re-reads are handled by the dialog's own lambda.
                if (streamedWidget && data.findGroup("auth_required").has_value())
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
                if (data.findGroup("auth_required").has_value()) {
                    // Re-entry guard: on wrong CAN retry, requestDataWithCredentials emits
                    // cardDataReady with auth_required + error groups. Route the error to
                    // the existing widget instead of creating a duplicate.
                    if (auto* existing = qobject_cast<EMRTDAuthWidget*>(it->second.widget)) {
                        if (auto errGroup = data.findGroup("error")) {
                            auto errMsg = LibreSCRS::Plugin::getFieldValue(data, errGroup, "error");
                            existing->onAuthFailed(errMsg.isEmpty() ? qtTrId("lc-error-auth-failed") : errMsg);
                        }
                        return;
                    }

                    auto* authWidget = new EMRTDAuthWidget(this);
                    auto paceFlag =
                        LibreSCRS::Plugin::getFieldValue(data, data.findGroup("auth_required"), "pace_supported");
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
                                    auto* ts = new TokenSection(
#ifdef LIBRECELIK_SIGNING_ENABLED
                                        trustService ? trustService->trustStore() : nullptr,
#else
                                        nullptr,
#endif
                                        self);
                                    ts->setHeaderColor(QColor(230, 135, 60));
                                    ts->setHeaderHeight(56);
                                    ts->setExpanded(!visible);
#ifdef LIBRECELIK_SIGNING_ENABLED
                                    ts->setReaderName(reader);
#endif
                                    pkiWidget = ts;
                                }
                                connectPKISignals(asyncReader, pkiWidget, reader);
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
                        auto* ts = new TokenSection(
#ifdef LIBRECELIK_SIGNING_ENABLED
                            trustService ? trustService->trustStore() : nullptr,
#else
                            nullptr,
#endif
                            self);
                        ts->setHeaderColor(QColor(230, 135, 60));
                        ts->setHeaderHeight(56);
                        ts->setExpanded(!visible2);
#ifdef LIBRECELIK_SIGNING_ENABLED
                        ts->setReaderName(reader);
#endif
                        pkiWidget = ts;
                    }
                    connectPKISignals(asyncReader, pkiWidget, reader);

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
            [this, reader, isSpinner, replaceWidget](const QString& cardType,
                                                     const LibreSCRS::Plugin::CardFieldGroup& group) {
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
    qCDebug(lcProbeQuieting) << "LC-REMOVE reader=" << QString::fromStdString(reader)
                             << "hadAsync=" << (it != activeReaders.end());
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
    // waiting for a PC/SC timeout. SCardCancel does not cancel an in-flight
    // SCardTransmit on any PC/SC implementation — see AsyncCardReader::cancel
    // comment for the rationale — so only cooperative stopRequested +
    // off-thread wait keeps the UI responsive.
    //
    // Threading invariant: clearPluginCredentials reads activePlugin
    // (std::shared_ptr<CardPlugin>), which is also written by the worker's
    // queued lambdas posted via QMetaObject::invokeMethod (Qt::QueuedConnection)
    // from requestData / requestDataWithCredentials. asyncReader->disconnect()
    // above only severs Qt signal/slot connections — it does NOT cancel
    // already-posted queued invokeMethod calls. So a worker that finished
    // just before initiateCancel() may still have a queued lambda Q1 in the
    // GUI event loop that runs `activePlugin = candidate`. Reading activePlugin
    // from the cleanup thread concurrent with that queued write is UB
    // (concurrent shared_ptr read+write).
    //
    // Discipline: we wait for pending async workers off-thread (the slow path,
    // which can take seconds while a PC/SC SCardTransmit times out), then
    // marshal clearPluginCredentials back to the GUI thread. The GUI thread
    // owns activePlugin and processes posted events in FIFO order, so any
    // straggler queued write from the worker has run by the time our cleanup
    // lambda executes. deleteLater() is posted from the cleanup lambda itself
    // (after clearPluginCredentials) so it cannot race ahead of the credential
    // clear: a deleteLater() queued earlier (e.g. on QThread::finished) would
    // be delivered to the GUI event loop and could destroy asyncReader before
    // our credential-clear lambda ran.
    auto* cleanupThread = QThread::create([asyncReader]() {
        asyncReader->waitForPendingAsync();
        QMetaObject::invokeMethod(
            asyncReader,
            [asyncReader]() {
                try {
                    asyncReader->clearPluginCredentials();
                } catch (...) {
                    // Card already removed — credentials are moot
                }
                asyncReader->deleteLater();
            },
            Qt::QueuedConnection);
    });
    connect(cleanupThread, &QThread::finished, cleanupThread, &QObject::deleteLater);
    cleanupThread->start();

    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);
    if (activeReaders.empty()) {
        ui->stackedWidget->setCurrentIndex(0);
        ui->statusbar->clearMessage();
    }
}

void LibreCelik::connectPKISignals(AsyncCardReader* reader, QWidget* pkiWidget, const std::string& readerName)
{
    auto* tokenSection = qobject_cast<TokenSection*>(pkiWidget);
    if (!tokenSection)
        return;

    // Close any open modal child dialogs (cert viewer, change-PIN, signing
    // wizard) hanging off this TokenSection when ITS card is pulled. Without
    // this, removeReader's deleteLater() reaps tokenSection mid-exec() of
    // its child QDialog -> dialog destroyed while its modal event loop is
    // unwound on the stack -> crash. We filter on the bound readerName so
    // pulling another reader's card does not affect dialogs in this token
    // section. We use done(QDialog::Rejected) (public) rather than reject()
    // (protected slot).
    const QString readerNameQ = QString::fromStdString(readerName);
    connect(this, &LibreCelik::cardRemoved, tokenSection, [tokenSection, readerNameQ](const QString& removed) {
        if (removed != readerNameQ)
            return;
        const auto dialogs = tokenSection->findChildren<QDialog*>();
        for (QDialog* d : dialogs)
            d->done(QDialog::Rejected);
    });

    connect(reader, &AsyncCardReader::tokenInfoReady, tokenSection, &TokenSection::setTokenInfo);
    connect(reader, &AsyncCardReader::certificatesReady, tokenSection, &TokenSection::setCertificates);
    connect(reader, &AsyncCardReader::certificatesReady, reader, &AsyncCardReader::requestPINList);
    connect(reader, &AsyncCardReader::pinListReady, tokenSection, &TokenSection::setPINList);
    // Qt::QueuedConnection is load-bearing: the emitter
    // (TokenSection::onContextMenu) lives inside a stack-allocated QMenu's
    // exec() loop. A direct connection would open ChangePinDlg::exec()
    // *while* the QMenu is still on the call stack — if the user then
    // pulls the card, the deferred-delete cascade for the reader's
    // QScrollArea reaps TokenSection's child list, including the
    // stack-allocated QMenu, producing a free()-on-stack-pointer abort
    // (POINTER_BEING_FREED_WAS_NOT_ALLOCATED on macOS). Queued delivery
    // posts the slot invocation back to the event loop, so the QMenu's
    // exec() unwinds and the menu is gone from the child list before the
    // dialog opens its own nested loop.
    connect(
        tokenSection, &TokenSection::changePINRequested, this,
        [this, reader](uint8_t pinRef, const QString& pinLabel, bool isTransport, int minLength, int maxLength) {
            QWidget* self = this;
            auto dlg = std::make_unique<ChangePinDlg>(pinLabel, isTransport, minLength, maxLength, self);
            connect(dlg.get(), &ChangePinDlg::pinChangeRequested, reader,
                    [reader, pinRef](const LibreSCRS::Secure::String& oldPin, const LibreSCRS::Secure::String& newPin) {
                        reader->requestChangePIN(pinRef, oldPin, newPin);
                    });
            connect(reader, &AsyncCardReader::pinStatusReady, dlg.get(), &ChangePinDlg::onPinRetriesLeftRead);
            connect(reader, &AsyncCardReader::pinChangeResult, dlg.get(),
                    [dlg = dlg.get()](bool success, int retriesLeft, const QString& errorMessage) {
                        if (success)
                            dlg->onPinChangeSuccess();
                        else
                            dlg->onPinChangeFailed(retriesLeft, retriesLeft == 0, errorMessage);
                    });
            reader->requestPINTriesLeft(pinRef);
            dlg->exec();
        },
        Qt::QueuedConnection);

#ifdef LIBRECELIK_SIGNING_ENABLED
    connect(
        tokenSection, &TokenSection::signRequested, this,
        [this](const LibreSCRS::Plugin::CertificateData& cert, const std::string& readerName) {
            if (!signingService)
                return;

            // Resolve the plugin BEFORE opening a fresh CardSession — if
            // this reader has no PKI-capable plugin bound (or the
            // AsyncCardReader entry vanished because the card was just
            // ejected), we want to fail without paying for an SCardConnect.
            //
            // Use the PKI plugin AsyncCardReader has already bound to this
            // reader — it's the plugin that produced @p cert, so the PIN
            // it will route through `card->verifyPIN` lands in the same
            // applet/slot the certificate lives in. Re-deriving via
            // `findAllCandidates(...).front()` was non-deterministic for
            // cards lacking a declared ATR (PKCS#15 generic, OpenSC
            // fallback), and even when correct it carried no guarantee
            // that the chosen plugin matched the one used for read.
            auto activeIt = activeReaders.find(readerName);
            if (activeIt == activeReaders.end() || !activeIt->second.reader) {
                qCWarning(lcSmartCard) << "signRequested: no AsyncCardReader bound to"
                                       << QString::fromStdString(readerName);
                return;
            }
            auto cardPlugin = activeIt->second.reader->signingPlugin();
            if (!cardPlugin) {
                qCWarning(lcSmartCard)
                    << "signRequested: card in" << QString::fromStdString(readerName)
                    << "has no PKI+PinManagement-capable plugin bound (cert came from a non-signing plugin?)";
                return;
            }
            qCInfo(lcSmartCard) << "signRequested: using plugin" << QString::fromStdString(cardPlugin->pluginId())
                                << "(probePriority" << cardPlugin->probePriority() << ") for reader"
                                << QString::fromStdString(readerName);

            // Open a fresh CardSession scoped to the wizard. This is
            // intentionally NOT the same session AsyncCardReader holds —
            // CardSession is not designed for shared concurrent use, and
            // the wizard's PIN/sign flow needs its own handle on the
            // reader for the duration of `wizard.exec()`.
            std::shared_ptr<LibreSCRS::SmartCard::CardSession> session;
            {
                auto opened = LibreSCRS::SmartCard::CardSession::open(readerName);
                if (!opened.has_value()) {
                    const auto& openErr = opened.error();
                    const std::string diag = openErr.diagnosticDetail.value_or(std::string{"?"});
                    qCWarning(lcSmartCard) << "signRequested: cannot open CardSession for"
                                           << QString::fromStdString(readerName) << ":" << QString::fromStdString(diag);
                    return;
                }
                session = std::make_shared<LibreSCRS::SmartCard::CardSession>(std::move(*opened));
            }

            SigningWizard wizard(cert, readerName, signingService, std::move(cardPlugin), std::move(session), this);
            // Per-request TSA override now flows via SigningRequest::tsaOverride in
            // SignPage; the service-level TsaProvider installed at startup from
            // SigningConfiguration remains the fallback when no override is set.
            // No more per-wizard service-state mutation or RAII restore.
            //
            // Auto-close on removal of the same card mid-sign: the wizard
            // stays QObject-decoupled from LibreCelik (the wizard unit-test
            // target does not link our MOC), so the connect lives here at
            // the call site. We filter by readerName so other readers'
            // events (one of three inserted cards being ejected) do not
            // reject this wizard. The signal is emitted from
            // onCardEventReceived BEFORE the AsyncCardReader for this
            // reader is torn down so any in-flight signing worker sees a
            // clean cancel rather than a torn SCardConnect.
            const QString readerNameQ = QString::fromStdString(readerName);
            connect(this, &LibreCelik::cardRemoved, &wizard, [&wizard, readerNameQ](const QString& removed) {
                if (removed == readerNameQ)
                    wizard.done(QDialog::Rejected);
            });
            wizard.exec();
        },
        Qt::QueuedConnection); // see changePINRequested above for the same hazard
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
    AboutDialog dlg(this);
    dlg.exec();
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
