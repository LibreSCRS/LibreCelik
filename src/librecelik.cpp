// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "config.h"
#include "utils/libreceliklog.h"
#include "smartcard/smartcardreaderlistener.h"
#include "ui_librecelik.h"

#include <QApplication>
#include <QEvent>
#include <QLocale>
#include <QPixmap>
#include <QSettings>

LibreCelik::LibreCelik(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LibreCelik)
{
    qCDebug(libreSCRSGeneral, "Setting up GUI");

    // Install translator BEFORE setupUi so the initial UI render is translated.
    // changeEvent is guarded by m_uiReady to avoid calling retranslateUi before
    // setupUi has run.
    QSettings settings("LibreSCRS", "LibreCelik");
    QString locale = settings.value("language", QString()).toString();

    if (!loadLanguage(locale)) {
        locale.clear();
        for (const QString &l : QLocale::system().uiLanguages()) {
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
    m_uiReady = true;
    updateLogo();

    ui->stackedWidget->setCurrentIndex(0);

    connect(ui->readerComboBox, &QComboBox::currentIndexChanged,
            ui->readerStackedWidget, &QStackedWidget::setCurrentIndex);

    ui->statusbar->hide();
    ui->menubar->hide();

    // Set combobox to match the locale that was actually loaded.
    // Connect the signal AFTER setCurrentIndex to avoid a spurious onLanguageChanged call.
    int langIndex = locale.startsWith("sr") ? 1 : 0;
    ui->languageComboBox->setCurrentIndex(langIndex);
    updateAboutText();

    connect(ui->languageComboBox, &QComboBox::currentIndexChanged,
            this, &LibreCelik::onLanguageChanged);

    connect(&SmartCardReaderListener::instance(), &SmartCardReaderListener::smartCardReaderEventOccured, this, &LibreCelik::onCardEventReceived);
    connect(&SmartCardReaderListener::instance(), &SmartCardReaderListener::smartCardReaderEnumerationChanged, this, &LibreCelik::onSmartCardReaderEnumerationChanged);
}

void LibreCelik::updateAboutText()
{
    ui->aboutLabel->setText(
        QString("<br><br>") +
        qtTrId("lc-main-about-librecelik").arg(LIBRECELIK_VERSION) +
        QString("<br>") +
        qtTrId("lc-main-about-libremiddleware").
#if defined(LIBRECELIK_LOCAL_MIDDLEWARE_VERSION) && LIBRECELIK_LOCAL_MIDDLEWARE_VERSION
                                arg("LOCAL"));
#else
                                arg(LIBRECELIK_MIDDLEWARE_VERSION));
#endif
}

void LibreCelik::updateLogo()
{
    const QString resource = m_locale.startsWith("sr")
        ? ":/images/LibreSCCelikLogo.png"
        : ":/images/LibreSCCelikLogoLatin.png";
    ui->label->setPixmap(QPixmap(resource));
}

bool LibreCelik::loadLanguage(const QString& locale)
{
    if (locale.isEmpty())
        return false;
    QApplication::removeTranslator(&translator);
    if (translator.load(":/i18n/LibreCelik_" + locale)) {
        QApplication::installTranslator(&translator);
        m_locale = locale;
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
}

void LibreCelik::changeEvent(QEvent* event)
{
    if (event->type() == QEvent::LanguageChange && m_uiReady) {
        ui->retranslateUi(this);
        updateAboutText();
        updateLogo();   // retranslateUi resets the pixmap; restore the correct one
    }
    QMainWindow::changeEvent(event);
}

void LibreCelik::onCardEventReceived(const SmartCardEvent& sce)
{
    qCDebug(libreSCRSGeneral) << "SmartCardEvent: " << sce.eventType << " received on reader:  " << sce.readerName;
    if (sce.eventType == SmartCardEvent::CardInserted)
    {
        addNewReader(sce.readerName);
    }
    if (sce.eventType == SmartCardEvent::CardRemoved)
    {
        removeReader(sce.readerName);
    }
}

void LibreCelik::onSmartCardReaderEnumerationChanged(const QStringList& scrNames)
{
    std::vector<std::string> readers;
    for(auto const& reader: documentReaders)
        readers.push_back(reader.first);

    // Remove unplugged readers
    std::vector<std::string> toRemove;
    std::set_difference(std::begin(readers), std::end(readers), std::begin(scrNames), std::end(scrNames), std::inserter(toRemove, std::begin(toRemove)));
    for (const auto& scrName : toRemove)
    {
        removeReader(scrName);
    }
}

void LibreCelik::addNewReader(std::string reader)
{
    if (documentReaders.count(reader))
        return;

    Document* document = Document::CreateDocument(reader, this);
    if (!document)
        return;

    int idx = ui->readerStackedWidget->addWidget(document);
    ui->readerComboBox->addItem(QString::fromStdString(reader));
    ui->readerComboBox->setCurrentIndex(idx);
    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);

    documentReaders[reader] = document;
    ui->stackedWidget->setCurrentIndex(1);
}

void LibreCelik::removeReader(std::string reader)
{
    auto it = documentReaders.find(reader);
    if (it == documentReaders.end())
        return;

    Document* doc = it->second;
    int idx = ui->readerStackedWidget->indexOf(doc);
    ui->readerComboBox->removeItem(idx);
    ui->readerStackedWidget->removeWidget(doc);
    doc->deleteLater();
    documentReaders.erase(it);

    ui->readerComboBox->setVisible(ui->readerComboBox->count() > 1);
    if (documentReaders.empty())
        ui->stackedWidget->setCurrentIndex(0);
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
