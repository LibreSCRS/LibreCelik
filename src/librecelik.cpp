// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "librecelik.h"
#include "utils/libreceliklog.h"
#include "smartcard/smartcardreaderlistener.h"
#include "ui_librecelik.h"

LibreCelik::LibreCelik(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::LibreCelik)
{
    qCDebug(libreSCRSGeneral, "Setting up GUI");
    ui->setupUi(this);

    ui->toolBox->widget(0)->deleteLater();

    // Hide Tokens until it is implemented
    //ui->tabWidget->setTabEnabled(1,false);
    ui->tabWidget->setTabVisible(1,false);

    ui->stackedWidget->setCurrentIndex(0);

    ui->statusbar->hide();
    ui->menubar->hide();

    connect(&SmartCardReaderListener::instance(), &SmartCardReaderListener::smartCardReaderEventOccured, this, &LibreCelik::onCardEventReceived);
    connect(&SmartCardReaderListener::instance(), &SmartCardReaderListener::smartCardReaderEnumerationChanged, this, &LibreCelik::onSmartCardReaderEnumerationChanged);
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
    if (documentReaders.find(reader) == documentReaders.end())
    {
        Document* document = Document::CreateDocument(reader, this);
        ui->toolBox->insertItem(0, document, QString(reader.c_str()));
        ui->toolBox->setCurrentIndex(0);
        documentReaders[reader] = document;
    }
    if (documentReaders.size() > 0)
        ui->stackedWidget->setCurrentIndex(1);
}

void LibreCelik::removeReader(std::string reader)
{
    if (documentReaders.find(reader) != documentReaders.end())
    {
        ui->toolBox->removeItem(ui->toolBox->indexOf(documentReaders[reader]));
        documentReaders[reader]->deleteLater();
        documentReaders.erase(reader);
    }
    if (documentReaders.empty())
        ui->stackedWidget->setCurrentIndex(0);
}

LibreCelik::~LibreCelik()
{
    delete ui;
}
