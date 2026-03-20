// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "config.h"
#include "document/eid/changepindlg.h"
#include "document/tokensection.h"
#include "smartcard/smartcardreaderlistener.h"
#include "ui_librecelik.h"
#include "utils/libreceliklog.h"

#include <smartcard/pcsc_connection.h>

#include <QApplication>
#include <QEvent>
#include <QLocale>
#include <QMenu>
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
    updateWelcomeChips();

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

void LibreCelik::updateWelcomeChips()
{
    ui->chipEid->setText(qtTrId("lc-eid-title-serbian"));
    ui->chipForeigner->setText(qtTrId("lc-eid-title-foreigner"));
    ui->chipVehicle->setText(qtTrId("lc-vehicle-title"));
    ui->chipHealth->setText(qtTrId("lc-health-title"));
    ui->chipPks->setText(qtTrId("lc-pks-title"));
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
        updateWelcomeChips();
        // retranslateUi resets the button to "English"; restore the actual locale.
        ui->languageButton->setText(locale.startsWith("sr") ? "Српски" : "English");
    }
    QMainWindow::changeEvent(event);
}

void LibreCelik::onCardEventReceived(const SmartCardEvent& sce)
{
    qCDebug(libreSCRSGeneral) << "SmartCardEvent: " << sce.eventType
                              << " received on reader:  " << QString::fromStdString(sce.readerName);
    if (sce.eventType == SmartCardEvent::CardInserted) {
        addNewReader(sce.readerName);
    }
    if (sce.eventType == SmartCardEvent::CardRemoved) {
        removeReader(sce.readerName);
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

    connect(asyncReader, &AsyncCardReader::cardDataReady, this,
            [this, asyncReader, reader](const plugin::CardData& data) {
                auto* guiPlugin = guiPluginRegistry.findByCardType(QString::fromStdString(data.cardType));
                if (!guiPlugin) {
                    ui->statusbar->show();
                    ui->statusbar->showMessage(qtTrId("lc-reader-unsupported-card"));
                    return;
                }

                QWidget* topWidget = guiPlugin->createWidget(data, this);

                if (asyncReader->currentPlugin()->supportsPKI()) {
                    auto* pkiWidget = guiPlugin->createPKIWidget(this);
                    if (!pkiWidget) {
                        auto* ts = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, this);
                        ts->setPINVisible(true);
                        pkiWidget = ts;
                    }
                    connectPKISignals(asyncReader, pkiWidget);

                    auto* container = new QWidget(this);
                    auto* layout = new QVBoxLayout(container);
                    layout->setContentsMargins(0, 0, 0, 0);
                    layout->addWidget(topWidget);
                    layout->addWidget(pkiWidget);
                    layout->addItem(new QSpacerItem(20, 0, QSizePolicy::Minimum, QSizePolicy::Expanding));
                    topWidget = container;

                    asyncReader->requestCertificates();
                    asyncReader->requestPINTriesLeft();
                }

                int idx = ui->readerStackedWidget->addWidget(topWidget);
                ui->readerComboBox->addItem(QString::fromStdString(reader));
                ui->readerComboBox->setCurrentIndex(idx);
                ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);

                activeReaders[reader] = {asyncReader, topWidget};
                ui->stackedWidget->setCurrentIndex(1);
            });

    connect(asyncReader, &AsyncCardReader::errorOccurred, this, [this](const QString& msg) {
        ui->statusbar->show();
        ui->statusbar->showMessage(msg, 5000);
    });

    asyncReader->requestData();
}

void LibreCelik::removeReader(std::string reader)
{
    auto it = activeReaders.find(reader);
    if (it == activeReaders.end())
        return;

    auto& [asyncReader, widget] = it->second;
    int idx = ui->readerStackedWidget->indexOf(widget);
    ui->readerComboBox->removeItem(idx);
    ui->readerStackedWidget->removeWidget(widget);
    widget->deleteLater();
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
