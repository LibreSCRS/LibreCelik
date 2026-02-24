// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QBuffer>
#include <QDate>
#include "utils/libreceliklog.h"
#include "utils/printmanager.h"
#include "eid.h"
#include "eidtextdocument.h"
#include "eidreader.h"
#include "changepindlg.h"
#include "certificate/certificateviewerdlg.h"
#include "config.h"
#include "ui_eid.h"

EId::EId(std::string reader, QWidget *parent)
    : Document(parent)
    , ui(new Ui::EId)
{
    ui->setupUi(this);

    eidReader = std::make_unique<EIdReader>(reader);

    connect(eidReader.get(), &EIdReader::cardTypeRead, this, &EId::cardTypeReceived);
    connect(eidReader.get(), &EIdReader::fixedPersonalDataRead, this, &EId::fixedPersonalDataReceived);
    connect(eidReader.get(), &EIdReader::variablePersonalDataRead, this, &EId::variablePersonalDataReceived);
    connect(eidReader.get(), &EIdReader::documentDataRead, this, &EId::documentDataReceived);
    connect(eidReader.get(), &EIdReader::photoDataRead, this, &EId::photoDataReceived);
    connect(eidReader.get(), &EIdReader::cardVerificationResultRead, this, &EId::cardVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::fixedVerificationResultRead, this, &EId::fixedVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::variableVerificationResultRead, this, &EId::variableVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::certificateDataRead, this, &EId::certificateDataReceived);

    connect(eidReader.get(), &EIdReader::readingStarted, [this](){
        ui->toolButton->setEnabled(false);
        ui->changePinButton->setEnabled(false);
        ui->certificatesButton->setEnabled(false);
    });
    connect(eidReader.get(), &EIdReader::readingFinished, [this](){
        ui->toolButton->setEnabled(true);
        ui->changePinButton->setEnabled(true);
    });

    certFolderPath = LIBRECELIK_CERTIFICATES_DIR;

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

void EId::cardTypeReceived(const eidcard::CardType& data)
{
    qCDebug(libreSCRSGeneral) << "Card type received" << static_cast<int>(data);
    cardType = data;
    switch (cardType)
    {
        case eidcard::CardType::Unknown:
        case eidcard::CardType::Apollo2008:
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
        case eidcard::CardType::Gemalto2014:
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
        case eidcard::CardType::ForeignerIF2020:
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

void EId::fixedPersonalDataReceived(const eidcard::FixedPersonalData& data)
{
    fixedPersonalData = data;
    ui->nameLineEdit_3->setText(QString::fromStdString(data.givenName));
    ui->surnameLineEdit_3->setText(QString::fromStdString(data.surname));
    ui->parentNameLineEdit_3->setText(QString::fromStdString(data.parentGivenName));
    ui->genderLineEdit_3->setText(QString::fromStdString(data.sex));
    ui->jmbgLineEdit_3->setText(QString::fromStdString(data.personalNumber));
    ui->dateOfBirthLineEdit_3->setText(QString::fromStdString(data.dateOfBirth));
    ui->statusOfForeignerLineEdit_3->setText(QString::fromStdString(data.statusOfForeigner));
    ui->placeOfBirthLineEdit_3->setText(assemblePlaceOfBirth(data));
    ui->nationalityLineEdit_3->setText(QString::fromStdString(data.nationalityFull));
}

void EId::variablePersonalDataReceived(const eidcard::VariablePersonalData& data)
{
    variablePersonalData = data;
    ui->addressLineEdit_3->setText(assembleAddress(data));

    auto addressDate = QString::fromStdString(data.addressDate);
    if (addressDate != "01.01.0001")
    {
        showLabelAndLineEdit(ui->dateOfAddressChangeLabel_3, ui->dateOfAddressChangeLineEdit_3, true);
        ui->dateOfAddressChangeLineEdit_3->setText(addressDate);
    }
    else
    {
        showLabelAndLineEdit(ui->dateOfAddressChangeLabel_3, ui->dateOfAddressChangeLineEdit_3, false);
    }
}

void EId::documentDataReceived(const eidcard::DocumentData& data)
{
    documentData = data;
    ui->documentNumberLineEdit_3->setText(QString::fromStdString(data.docRegNo));

    auto expiryDate = QString::fromStdString(data.expiryDate);
    QDate receivedDate = QDate::fromString(expiryDate, "dd.MM.yyyy");
    QDate currentDate = QDate::currentDate();
    QPalette palette = ui->validToLineEdit_3->palette();
    if (receivedDate.isValid() && receivedDate < currentDate)
    {
        palette.setColor(QPalette::Text, QColor(0xe6, 0x87, 0x3c));
    }
    ui->validToLineEdit_3->setPalette(palette);

    ui->validToLineEdit_3->setText(expiryDate);
    ui->dateOfIssuanceLineEdit_3->setText(QString::fromStdString(data.issuingDate));
    ui->documentIssuerLineEdit_3->setText(QString::fromStdString(data.issuingAuthority));
}

void EId::photoDataReceived(const eidcard::PhotoData& data)
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

void EId::updateVerificationIcons(const eidcard::VerificationResult& data, QLabel* iconLabel)
{
    switch (data)
    {
        case eidcard::VerificationResult::Unknown:
        {
            iconLabel->setPixmap(QPixmap(":/images/grey_question_mark.png"));
            break;
        }
        case eidcard::VerificationResult::Valid:
        {
            iconLabel->setPixmap(QPixmap(":/images/green_checked.png"));
            break;
        }
        case eidcard::VerificationResult::Invalid:
        {
            iconLabel->setPixmap(QPixmap(":/images/red_exclamation.png"));
            break;
        }
    }
}

void EId::cardVerificationResultReceived(const eidcard::VerificationResult& data)
{
    updateVerificationIcons(data, ui->group1Label_3);
}

void EId::fixedVerificationResultReceived(const eidcard::VerificationResult& data)
{
    updateVerificationIcons(data, ui->group2Label_3);
}

void EId::variableVerificationResultReceived(const eidcard::VerificationResult& data)
{
    updateVerificationIcons(data, ui->group3Label_3);
}

QString EId::assembleAddress(const eidcard::VariablePersonalData& vpd) const
{
    QStringList parts;
    parts << QString::fromStdString(vpd.place)
          << QString::fromStdString(vpd.community)
          << QString::fromStdString(vpd.street)
          << QString::fromStdString(vpd.houseNumber);
    auto address = parts.join(", ");
    auto floor = QString::fromStdString(vpd.floor);
    auto apartmentNumber = QString::fromStdString(vpd.apartmentNumber);
    if (!floor.isEmpty())
        address += "/" + floor;
    if (!apartmentNumber.isEmpty())
        address += "/" + apartmentNumber;
    return address;
}

QString EId::assemblePlaceOfBirth(const eidcard::FixedPersonalData& fpd) const
{
    QStringList parts;
    parts << QString::fromStdString(fpd.placeOfBirth)
          << QString::fromStdString(fpd.communityOfBirth)
          << QString::fromStdString(fpd.stateOfBirth);
    return parts.join(", ");
}

QString EId::getBase64Photo()
{
    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    ui->pictureLabel_3->pixmap(Qt::ReturnByValue).save(&buffer, "PNG");

    return bytes.toBase64();
}

void EId::certificateDataReceived(const eidcard::CertificateList& data)
{
    certificateList = data;
    ui->certificatesButton->setEnabled(!certificateList.empty());
}

void EId::on_certificatesButton_clicked()
{
    if (certificateList.empty())
        return;

    certificateDialog = std::make_unique<CertificateViewerDlg>(certificateList, certFolderPath, this);
    certificateDialog->exec();
}

void EId::on_changePinButton_clicked()
{
    changePinDlg = std::make_unique<ChangePinDlg>(eidReader->getReaderName(), this);
    changePinDlg->exec();
}

void EId::on_toolButton_clicked()
{
    QString documentTemplate = ":/html/idcard.html";
    if (cardType == eidcard::CardType::ForeignerIF2020)
    {
        documentTemplate = ":/html/idcardIF2020.html";
    }
    auto address = assembleAddress(variablePersonalData);
    auto placeOfBirth = assemblePlaceOfBirth(fixedPersonalData);
    PrintManager::printDocument(EIdTextDocument(fixedPersonalData, variablePersonalData, documentData, address, placeOfBirth, getBase64Photo(), documentTemplate), tr("Print Document"));
}
