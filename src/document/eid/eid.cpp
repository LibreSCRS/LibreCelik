// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QBuffer>
#include <QDate>
#include "utils/libreceliklog.h"
#include "utils/printmanager.h"
#include "eid.h"
#include "eidtextdocument.h"
#include "eidreader.h"
#include "ui_eid.h"

EId::EId(std::string reader, QWidget *parent)
    : Document(parent)
    , ui(new Ui::EId)
{
    ui->setupUi(this);

    eidReader = std::make_unique<EIdReader>(reader);

    connect(eidReader.get(), &EIdReader::cardVersionRead, this, &EId::cardVersionReceived);
    connect(eidReader.get(), &EIdReader::fixedPersonalDataRead, this, &EId::fixedPersonalDataReceived);
    connect(eidReader.get(), &EIdReader::variablePersonalDataRead, this, &EId::varibalePersonalDataReceived);
    connect(eidReader.get(), &EIdReader::documentDataRead, this, &EId::documentDataReceived);
    connect(eidReader.get(), &EIdReader::photoDataRead, this, &EId::photoDataReceived);
    connect(eidReader.get(), &EIdReader::cardSignatureVerificationResultRead, this, &EId::cardSignatureVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::fixedSignatureVerificationResultRead, this, &EId::fixedSignatureVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::variableSignatureVerificationResultRead, this, &EId::variableSignatureVerificationResultReceived);

    connect(eidReader.get(), &EIdReader::readingStarted, [this](){
        ui->toolButton->setEnabled(false);
    });
    connect(eidReader.get(), &EIdReader::readingFinished, [this](){
        ui->toolButton->setEnabled(true);
    });

    eidReader->requestData();
}

EId::~EId()
{
    eidReader.reset();
    delete ui;
}

void EId::showLabelAndLineEdit(QLabel* label, QLineEdit* lineEdit, bool show)
{
    if (show)
    {
        label->show();
        lineEdit->show();
    }
    else
    {
        label->hide();
        lineEdit->hide();
    }
}

void EId::cardVersionReceived(const CelikAPI::CardVersion& data)
{
    qCDebug(libreSCRSGeneral) << "Card version received" << static_cast<int> (data);
    cardVersion = data;
    switch (cardVersion)
    {
        case CelikAPI::CardVersion::Unknown:
        case CelikAPI::CardVersion::Card2008:
        case CelikAPI::CardVersion::Card2014:
        {
            ui->citizenGroupBox_3->setTitle(tr("Citizen Data"));
            ui->jmbgLabel_3->setText(tr("JMBG"));
            ui->lkLabel_3->setText(tr("Identity card"));
            ui->addressLabel_3->setText(tr("Address"));
            showLabelAndLineEdit(ui->parentNameLabel_3, ui->parentNameLineEdit_3, true);
            showLabelAndLineEdit(ui->nationalityLabel_3, ui->nationalityLineEdit_3, false);
            showLabelAndLineEdit(ui->placeOfBirthLabel_3, ui->placeOfBirthLineEdit_3, true);
            showLabelAndLineEdit(ui->statusOfForeignerLabel_3, ui->statusOfForeignerLineEdit_3, false);
            break;
        }
        case CelikAPI::CardVersion::CardIF2020:
        {
            ui->citizenGroupBox_3->setTitle(tr("Foreigner Data"));
            ui->jmbgLabel_3->setText(tr("EBS"));
            ui->lkLabel_3->setText(tr("Identity card for foreigners"));
            ui->addressLabel_3->setText(tr("Address", "foreigner"));
            showLabelAndLineEdit(ui->parentNameLabel_3, ui->parentNameLineEdit_3, false);
            showLabelAndLineEdit(ui->nationalityLabel_3, ui->nationalityLineEdit_3, true);
            showLabelAndLineEdit(ui->placeOfBirthLabel_3, ui->placeOfBirthLineEdit_3, false);
            showLabelAndLineEdit(ui->statusOfForeignerLabel_3, ui->statusOfForeignerLineEdit_3, true);
            break;
        }
    }
}

void EId::fixedPersonalDataReceived(const CelikAPI::FixedPersonalData& data)
{
    fixedPersonalData = data;
    ui->nameLineEdit_3->setText(data.givenName);
    ui->surnameLineEdit_3->setText(data.surname);
    ui->parentNameLineEdit_3->setText(data.parentGivenName);
    ui->genderLineEdit_3->setText(data.sex);
    ui->jmbgLineEdit_3->setText(data.personalNumber);
    ui->dateOfBirthLineEdit_3->setText(data.dateOfBirth);
    ui->statusOfForeignerLineEdit_3->setText(data.statusOfForeigner);
    ui->placeOfBirthLineEdit_3->setText(data.placeOfBirth);
    ui->nationalityLineEdit_3->setText(data.nationalityFull);
}

void EId::varibalePersonalDataReceived(const CelikAPI::VariablePersonalData& data)
{
    variablePersonalData = data;
    ui->addressLineEdit_3->setText(data.address);

    if (data.addressDate != "01.01.0001")
    {
        showLabelAndLineEdit(ui->dateOfAddressChangeLabel_3, ui->dateOfAddressChangeLineEdit_3, true);
        ui->dateOfAddressChangeLineEdit_3->setText(data.addressDate);
    }
    else
    {
        showLabelAndLineEdit(ui->dateOfAddressChangeLabel_3, ui->dateOfAddressChangeLineEdit_3, false);
    }
}

void EId::documentDataReceived(const CelikAPI::DocumentData& data)
{
    documentData = data;
    ui->documentNumberLineEdit_3->setText(data.docRegNo);

    QDate receivedDate = QDate::fromString(data.expiryDate, "dd.MM.yyyy");
    QDate currentDate = QDate::currentDate();
    QPalette palette = ui->validToLineEdit_3->palette();
    if (receivedDate.isValid() && receivedDate < currentDate)
    {
        palette.setColor(QPalette::Text, QColor("#e6873c"));
    }
    ui->validToLineEdit_3->setPalette(palette);

    ui->validToLineEdit_3->setText(data.expiryDate);
    ui->dateOfIssuanceLineEdit_3->setText(data.issuingDate);
    ui->documentIssuerLineEdit_3->setText(data.issuingAuthority);
}

void EId::photoDataReceived(const CelikAPI::PhotoData& data)
{
    if (data.size() > 0)
    {
        QPixmap pixmap;
        pixmap.loadFromData(data.data(), data.size());
        ui->pictureLabel_3->setPixmap(pixmap);
    }
    else
    {
        ui->pictureLabel_3->setPixmap(QPixmap(QString::fromUtf8(":/images/user.png")));
    }
}

void EId::updateVerificationIcons(const CelikAPI::VerificationResult& data, QLabel* iconLabel)
{
    switch (data)
    {
        case CelikAPI::VerificationResult::Unknown:
        {
            iconLabel->setPixmap(QPixmap(":/images/grey_question_mark.png"));
            break;
        }
        case CelikAPI::VerificationResult::Good:
        {
            iconLabel->setPixmap(QPixmap(":/images/green_checked.png"));
            break;
        }

        case CelikAPI::VerificationResult::Bad:
        {
            iconLabel->setPixmap(QPixmap(":/images/red_exclamation.png"));
            break;
        }
    }
}

void EId::cardSignatureVerificationResultReceived(const CelikAPI::VerificationResult& data)
{
    updateVerificationIcons(data, ui->group1Label_3);
}

void EId::fixedSignatureVerificationResultReceived(const CelikAPI::VerificationResult& data)
{
    updateVerificationIcons(data, ui->group2Label_3);
}

void EId::variableSignatureVerificationResultReceived(const CelikAPI::VerificationResult& data)
{
    updateVerificationIcons(data, ui->group3Label_3);
}

QString EId::getBase64Photo()
{
    CelikAPI::PhotoData photo;
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    ui->pictureLabel_3->pixmap(Qt::ReturnByValue).save(&buffer, "PNG");

    return bytes.toBase64();
}

void EId::on_toolButton_clicked()
{
    QString documentTemplate = ":/html/idcard.html";
    if (cardVersion == CelikAPI::CardVersion::CardIF2020)
    {
        documentTemplate = ":/html/idcardIF2020.html";
    }
    PrintManager::printDocument(EIdTextDocument(fixedPersonalData, variablePersonalData, documentData, getBase64Photo(), documentTemplate), tr("Print Document"));
}

