// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "config.h"
#include "document/eid/changepindlg.h"
#include "document/emrtd/emrtdauthdlg.h"
#include "document/tokensection.h"
#include "plugin/carddatautils.h"
#include "smartcard/smartcardreaderlistener.h"
#include "ui_librecelik.h"
#include "utils/libreceliklog.h"

#include <smartcard/pcsc_connection.h>

#include <QApplication>
#include <QColor>
#include <QEvent>
#include <QLabel>
#include <QLocale>
#include <QMenu>
#include <QProgressBar>
#include <QScrollArea>
#include <QSettings>
#include <QSpacerItem>
#include <QTimer>
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

    // Load middleware card plugins
    middlewarePluginRegistry.loadPluginsFromDirectory(LIBREMIDDLEWARE_PLUGIN_DIR);

    // Load GUI widget plugins
    guiPluginRegistry.loadPluginsFromDirectory(LIBRECELIK_GUI_PLUGIN_DIR);

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
        // Dual-interface readers (e.g. OMNIKEY 5422) expose the same card on both slots.
        // We prefer the contact slot — it avoids CL communication instability.
        auto readerStr = QString::fromStdString(event.readerName);
        // Use '[' as delimiter — the bracketed part contains slot-specific info
        // (e.g. "5422CL" vs "5422"). Using '(' would include the brackets and fail to match.
        int delim = readerStr.indexOf('[');
        if (delim < 0)
            delim = readerStr.indexOf('(');
        auto basePrefix = (delim >= 0) ? readerStr.left(delim) : readerStr;

        if (readerStr.contains("CL")) {
            // CL slot detected — skip if contact slot of same reader is already active
            for (const auto& [name, _] : activeReaders) {
                auto activeName = QString::fromStdString(name);
                if (!activeName.contains("CL") && activeName.startsWith(basePrefix)) {
                    qCDebug(libreSCRSGeneral) << "Skipping CL slot — contact slot already active for same reader";
                    return;
                }
            }
        } else {
            // Contact slot detected — if CL slot of same reader is active, remove it first
            std::string clToRemove;
            for (const auto& [name, _] : activeReaders) {
                auto activeName = QString::fromStdString(name);
                if (activeName.contains("CL") && activeName.startsWith(basePrefix)) {
                    clToRemove = name;
                    break;
                }
            }
            if (!clToRemove.empty()) {
                qCDebug(libreSCRSGeneral)
                    << "Contact slot detected — removing CL slot:" << QString::fromStdString(clToRemove);
                removeReader(clToRemove);
            }
        }
        addNewReader(event.readerName);
    } else if (event.type == smartcard::MonitorEvent::Type::CardRemoved) {
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

    // Dual-interface: skip CL if contact slot of same physical reader is active.
    // This catches retries that bypass onCardEventReceived.
    auto readerStr = QString::fromStdString(reader);
    if (readerStr.contains("CL")) {
        int d = readerStr.indexOf('[');
        auto bp = readerStr.left(d >= 0 ? d : readerStr.indexOf('('));
        for (const auto& [name, _] : activeReaders) {
            auto an = QString::fromStdString(name);
            if (!an.contains("CL") && an.startsWith(bp)) {
                qCDebug(libreSCRSGeneral) << "Skipping CL — contact slot already active";
                return;
            }
        }
    }

    if (retryCount == 0) {
        // Fresh card event: defensively remove any stale widget left over from a
        // fast swap where CardRemoved wasn't emitted (no-op if nothing registered).
        removeReader(reader);
    } else if (activeReaders.count(reader)) {
        // Retry timer: a widget was created while this timer was pending — stop.
        return;
    }

    std::unique_ptr<smartcard::PCSCConnection> conn;
    std::vector<uint8_t> atr;
    try {
        conn = std::make_unique<smartcard::PCSCConnection>(reader);
        atr = conn->getATR();
    } catch (const std::exception&) {
        if (retryCount < 2) {
            QTimer::singleShot(300, this, [this, reader, retryCount]() { addNewReader(reader, retryCount + 1); });
        } else {
            ui->statusbar->show();
            ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
        }
        return;
    }

    auto candidates = middlewarePluginRegistry.findAllCandidates(atr, *conn);
    if (candidates.empty()) {
        if (retryCount < 2) {
            QTimer::singleShot(300, this, [this, reader, retryCount]() { addNewReader(reader, retryCount + 1); });
        } else {
            ui->statusbar->show();
            ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
        }
        return;
    }

    // A valid card is being added — clear any previous unsupported-card notice.
    ui->statusbar->clearMessage();

    auto* asyncReader = new AsyncCardReader(std::move(candidates), std::move(conn), this);

    // Show loading spinner immediately
    auto* spinnerWidget = new QWidget(this);
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
    auto isSpinner = [](QWidget* w) { return w && w->findChild<QProgressBar*>() != nullptr; };

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

                // eMRTD two-phase auth: keep spinner, open auth dialog over it
                if (data.findGroup("auth_required")) {
                    bool paceSupported =
                        (plugin::getFieldValue(data.findGroup("auth_required"), "pace_supported") == "true");
                    auto* dlg = new EMRTDAuthDlg(paceSupported, this);

                    connect(dlg, &EMRTDAuthDlg::credentialsEntered, asyncReader,
                            &AsyncCardReader::requestDataWithCredentials);

                    // Close dialog on first streaming group (auth succeeded, data arriving)
                    connect(
                        asyncReader, &AsyncCardReader::cardGroupReady, dlg,
                        [dlg](const QString& /*cardType*/, const plugin::CardFieldGroup& group) {
                            if (group.groupKey == "auth_required" || group.groupKey == "error")
                                return;
                            dlg->accept();
                        },
                        static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

                    // Phase 2 errors go to the dialog
                    connect(
                        asyncReader, &AsyncCardReader::cardDataReady, dlg,
                        [dlg](const plugin::CardData& newData) {
                            if (newData.findGroup("error")) {
                                auto errMsg = plugin::getFieldValue(newData.findGroup("error"), "error");
                                dlg->onAuthFailed(errMsg.isEmpty() ? QObject::tr("Authentication failed") : errMsg);
                                return;
                            }
                        },
                        static_cast<Qt::ConnectionType>(Qt::QueuedConnection | Qt::SingleShotConnection));

                    connect(asyncReader, &AsyncCardReader::errorOccurred, dlg, &EMRTDAuthDlg::onAuthFailed);

                    dlg->exec();
                    delete dlg;
                    return;
                }

                // Streaming already built the card widget — just append TokenSection
                if (streamedWidget) {
                    if (asyncReader->hasPKI()) {
                        auto* scrollArea = qobject_cast<QScrollArea*>(it->second.widget);
                        if (scrollArea && scrollArea->widget()) {
                            auto* containerLayout = qobject_cast<QVBoxLayout*>(scrollArea->widget()->layout());
                            if (containerLayout) {
                                auto* pkiWidget = guiPlugin->createPKIWidget(this);
                                if (!pkiWidget) {
                                    auto* ts = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, this);
                                    ts->setHeaderColor(QColor(230, 135, 60));
                                    ts->setHeaderHeight(56);
                                    ts->setPINVisible(true);
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

                QWidget* topWidget = guiPlugin->createWidget(data, this);

                if (asyncReader->hasPKI()) {
                    auto* pkiWidget = guiPlugin->createPKIWidget(this);
                    if (!pkiWidget) {
                        auto* ts = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, this);
                        ts->setHeaderColor(QColor(230, 135, 60));
                        ts->setHeaderHeight(56);
                        ts->setPINVisible(true);
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

    connect(asyncReader, &AsyncCardReader::errorOccurred, this, [this](const QString& msg) {
        ui->statusbar->show();
        ui->statusbar->showMessage(msg, 5000);
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

    auto& [asyncReader, widget] = it->second;
    if (widget) {
        int idx = ui->readerStackedWidget->indexOf(widget);
        ui->readerComboBox->removeItem(idx);
        ui->readerStackedWidget->removeWidget(widget);
        widget->deleteLater();
    }
    // Close eMRTD auth dialog if open (before disconnecting signals)
    if (auto* dlg = findChild<EMRTDAuthDlg*>()) {
        dlg->reject();
    }
    // Synchronously stop async threads so the PC/SC connection is released
    // before any new reader on the same physical card tries to connect.
    asyncReader->cancel();
    asyncReader->disconnect();
    asyncReader->deleteLater();
    activeReaders.erase(it);

    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);
    if (activeReaders.empty())
        ui->stackedWidget->setCurrentIndex(0);
}

void LibreCelik::connectPKISignals(AsyncCardReader* reader, QWidget* pkiWidget)
{
    auto* tokenSection = qobject_cast<TokenSection*>(pkiWidget);
    if (!tokenSection)
        return;

    connect(reader, &AsyncCardReader::certificatesReady, tokenSection, &TokenSection::setCertificates);
    // Chain PIN status request after certificates arrive — avoids blocking the
    // main thread with back-to-back futurePKI.wait() calls.
    connect(reader, &AsyncCardReader::certificatesReady, reader, &AsyncCardReader::requestPINTriesLeft);
    connect(reader, &AsyncCardReader::pinStatusReady, tokenSection, &TokenSection::setPINStatus);
    connect(tokenSection, &TokenSection::changePINRequested, this, [this, reader]() {
        auto dlg = std::make_unique<ChangePinDlg>(this);
        connect(dlg.get(), &ChangePinDlg::pinChangeRequested, reader, &AsyncCardReader::requestChangePIN);
        connect(reader, &AsyncCardReader::pinStatusReady, dlg.get(), &ChangePinDlg::onPinTriesLeftRead);
        connect(reader, &AsyncCardReader::pinChangeResult, dlg.get(),
                [dlg = dlg.get()](bool success, int triesLeft, const QString& errorMessage) {
                    if (success)
                        dlg->onPinChangeSuccess();
                    else
                        dlg->onPinChangeFailed(triesLeft, triesLeft == 0, errorMessage);
                });
        reader->requestPINTriesLeft();
        dlg->exec();
    });
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
