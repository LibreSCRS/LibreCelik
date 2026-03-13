// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QBuffer>
#include <QDate>
#include <QIcon>
#include <QPushButton>
#include <QResizeEvent>
#include <QVBoxLayout>
#include "utils/libreceliklog.h"
#include "utils/printmanager.h"
#include "eid.h"
#include "eidtextdocument.h"
#include "eidreader.h"
#include "changepindlg.h"
#include "document/tokensection.h"
#include "config.h"
#include "ui_eid.h"

EId::EId(std::string reader, QWidget* parent) : Document(parent), ui(new Ui::EId)
{
    ui->setupUi(this);
    ui->verticalLayout->setStretch(0, 1);

    ui->identityGroupBox->setHeaderHeight(56);

    printButton = new QPushButton(qtTrId("lc-print-button"));
    printButton->setIcon(QIcon(":/images/printer-1414.png"));
    printButton->setEnabled(false);
    ui->identityGroupBox->addHeaderWidget(printButton);

    tokenSection = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, ui->scrollAreaWidgetContents);
    tokenSection->setPINVisible(false);
    auto* scrollLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    scrollLayout->insertWidget(scrollLayout->count() - 1, tokenSection);

    eidReader = std::make_unique<EIdReader>(reader);

    connect(eidReader.get(), &EIdReader::cardTypeRead, this, &EId::cardTypeReceived);
    connect(eidReader.get(), &EIdReader::fixedPersonalDataRead, this, &EId::fixedPersonalDataReceived);
    connect(eidReader.get(), &EIdReader::variablePersonalDataRead, this, &EId::variablePersonalDataReceived);
    connect(eidReader.get(), &EIdReader::documentDataRead, this, &EId::documentDataReceived);
    connect(eidReader.get(), &EIdReader::photoDataRead, this, &EId::photoDataReceived);
    connect(eidReader.get(), &EIdReader::cardVerificationResultRead, this, &EId::cardVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::fixedVerificationResultRead, this, &EId::fixedVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::variableVerificationResultRead, this,
            &EId::variableVerificationResultReceived);
    connect(eidReader.get(), &EIdReader::certificateDataRead, tokenSection, &TokenSection::setCertificates);
    connect(eidReader.get(), &EIdReader::pinTriesLeftRead, tokenSection, &TokenSection::setPINStatus);
    connect(tokenSection, &TokenSection::changePINRequested, this, &EId::openChangePinDlg);

    connect(eidReader.get(), &EIdReader::readingStarted, [this]() { printButton->setEnabled(false); });
    connect(eidReader.get(), &EIdReader::readingFinished, [this]() { printButton->setEnabled(true); });
    connect(printButton, &QPushButton::clicked, this, &EId::printDocument);

    eidReader->requestData();
}

EId::~EId()
{
    eidReader.reset();
    delete ui;
}

void EId::applyCardTypeVisibility()
{
    bool isForeigner = (cardType == eidcard::CardType::ForeignerIF2020);
    ui->parentNameWidget->setVisible(!isForeigner);
    ui->nationalityWidget->setVisible(isForeigner);
    ui->placeOfBirthWidget->setVisible(!isForeigner);
    ui->statusOfForeignerWidget->setVisible(isForeigner);
    repositionAddressSection();
}

void EId::repositionAddressSection()
{
    auto* fieldsLayout = ui->verticalLayout_citizenFields;
    int photoMaxHeight = ui->pictureLabel_3->maximumHeight();

    // Temporarily move address into the fields column if not already there, so the
    // layout can compute the combined height in one consistent sizeHint() call.
    bool wasInColumn = m_addressInColumn;
    if (!m_addressInColumn) {
        ui->verticalLayout_citizen->removeWidget(ui->addressSection);
        fieldsLayout->insertWidget(fieldsLayout->count() - 1, ui->addressSection);
    }

    fieldsLayout->invalidate();
    bool fits = (fieldsLayout->sizeHint().height() <= photoMaxHeight);

    if (fits) {
        m_addressInColumn = true;
    } else {
        fieldsLayout->removeWidget(ui->addressSection);
        ui->verticalLayout_citizen->addWidget(ui->addressSection);
        m_addressInColumn = false;
    }
}

void EId::resizeEvent(QResizeEvent* event)
{
    Document::resizeEvent(event);
    repositionAddressSection();
}

void EId::cardTypeReceived(const eidcard::CardType& data)
{
    qCDebug(libreSCRSGeneral) << "Card type received" << static_cast<int>(data);
    cardType = data;
    bool hasPin = false;
    switch (cardType) {
    case eidcard::CardType::Unknown:
    case eidcard::CardType::Apollo2008: {
        ui->identityGroupBox->setTitle(qtTrId("lc-eid-title-serbian"));
        ui->citizenSection->setTitle(qtTrId("lc-eid-citizen-data"));
        ui->jmbgLabel_3->setText(qtTrId("lc-eid-label-jmbg"));
        ui->addressLabel_3->setText(qtTrId("lc-eid-label-address"));
        break;
    }
    case eidcard::CardType::Gemalto2014: {
        ui->identityGroupBox->setTitle(qtTrId("lc-eid-title-serbian"));
        ui->citizenSection->setTitle(qtTrId("lc-eid-citizen-data"));
        ui->jmbgLabel_3->setText(qtTrId("lc-eid-label-jmbg"));
        ui->addressLabel_3->setText(qtTrId("lc-eid-label-address"));
        hasPin = true;
        break;
    }
    case eidcard::CardType::ForeignerIF2020: {
        ui->identityGroupBox->setTitle(qtTrId("lc-eid-title-foreigner"));
        ui->citizenSection->setTitle(qtTrId("lc-eid-foreigner-data"));
        ui->jmbgLabel_3->setText(qtTrId("lc-eid-label-ebs"));
        ui->addressLabel_3->setText(qtTrId("lc-eid-label-address-foreigner"));
        hasPin = true;
        break;
    }
    }
    applyCardTypeVisibility();
    tokenSection->setPINVisible(hasPin);
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
    bool show = (addressDate != "01.01.0001");
    ui->dateOfAddressChangeWidget->setVisible(show);
    if (show)
        ui->dateOfAddressChangeLineEdit_3->setText(addressDate);
    repositionAddressSection();
}

void EId::documentDataReceived(const eidcard::DocumentData& data)
{
    documentData = data;
    ui->documentNumberLineEdit_3->setText(QString::fromStdString(data.docRegNo));

    auto expiryDate = QString::fromStdString(data.expiryDate);
    QDate receivedDate = QDate::fromString(expiryDate, "dd.MM.yyyy");
    QDate currentDate = QDate::currentDate();
    QPalette palette = ui->validToLineEdit_3->palette();
    if (receivedDate.isValid() && receivedDate < currentDate) {
        palette.setColor(QPalette::Text, QColor(0xe6, 0x87, 0x3c));
    }
    ui->validToLineEdit_3->setPalette(palette);

    ui->validToLineEdit_3->setText(expiryDate);
    ui->dateOfIssuanceLineEdit_3->setText(QString::fromStdString(data.issuingDate));
    ui->documentIssuerLineEdit_3->setText(QString::fromStdString(data.issuingAuthority));
}

void EId::photoDataReceived(const eidcard::PhotoData& data)
{
    if (data.size() > 0) {
        QPixmap pixmap;
        pixmap.loadFromData(data.data(), data.size());
        ui->pictureLabel_3->setPixmap(pixmap);
    } else {
        ui->pictureLabel_3->setPixmap(QPixmap(QString::fromUtf8(":/images/user.png")));
    }
}

void EId::updateVerificationIcons(const eidcard::VerificationResult& data, QLabel* iconLabel)
{
    switch (data) {
    case eidcard::VerificationResult::Unknown: {
        iconLabel->setPixmap(QPixmap(":/images/grey_question_mark.png"));
        break;
    }
    case eidcard::VerificationResult::Valid: {
        iconLabel->setPixmap(QPixmap(":/images/green_checked.png"));
        break;
    }
    case eidcard::VerificationResult::Invalid: {
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
    parts << QString::fromStdString(vpd.place) << QString::fromStdString(vpd.community)
          << QString::fromStdString(vpd.street) << QString::fromStdString(vpd.houseNumber);
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
    parts << QString::fromStdString(fpd.placeOfBirth) << QString::fromStdString(fpd.communityOfBirth)
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

void EId::openChangePinDlg()
{
    auto dlg = std::make_unique<ChangePinDlg>(this);
    connect(dlg.get(), &ChangePinDlg::pinChangeRequested, eidReader.get(), &EIdReader::requestChangePIN);
    connect(eidReader.get(), &EIdReader::pinTriesLeftRead, dlg.get(), &ChangePinDlg::onPinTriesLeftRead);
    connect(eidReader.get(), &EIdReader::pinChangeSuccess, dlg.get(), &ChangePinDlg::onPinChangeSuccess);
    connect(eidReader.get(), &EIdReader::pinChangeFailed, dlg.get(), &ChangePinDlg::onPinChangeFailed);
    eidReader->requestPINTriesLeft();
    dlg->exec();
}

void EId::printDocument()
{
    QString documentTemplate = ":/html/idcard.html";
    if (cardType == eidcard::CardType::ForeignerIF2020) {
        documentTemplate = ":/html/idcardIF2020.html";
    }
    auto address = assembleAddress(variablePersonalData);
    auto placeOfBirth = assemblePlaceOfBirth(fixedPersonalData);
    PrintManager::printDocument(EIdTextDocument(fixedPersonalData, variablePersonalData, documentData, address,
                                                placeOfBirth, getBase64Photo(), documentTemplate),
                                qtTrId("lc-eid-print-title"));
}
