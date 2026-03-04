// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QIcon>
#include <QPushButton>
#include <QVBoxLayout>
#include "utils/libreceliklog.h"
#include "utils/printmanager.h"
#include "health.h"
#include "healthtextdocument.h"
#include "healthreader.h"
#include "document/eid/changepindlg.h"
#include "document/tokensection.h"
#include "config.h"
#include "ui_health.h"

Health::Health(std::string reader, QWidget *parent)
    : Document(parent)
    , ui(new Ui::Health)
{
    ui->setupUi(this);
    ui->healthCardSection->setTitle(qtTrId("lc-health-title"));
    // healthCardSection (stretch=1) fills all space when expanded;
    // verticalSpacer_outer2 (stretch=0) absorbs the space when collapsed.
    ui->verticalLayout->setStretch(0, 1);

    ui->healthCardSection->setHeaderHeight(56);

    printButton = new QPushButton(qtTrId("lc-print-button"));
    printButton->setIcon(QIcon(":/images/printer-1414.png"));
    printButton->setEnabled(false);
    ui->healthCardSection->addHeaderWidget(printButton);

    tokenSection = new TokenSection(LIBRECELIK_CERTIFICATES_DIR, ui->scrollAreaWidgetContents);
    tokenSection->setPINVisible(true);
    auto* scrollLayout = qobject_cast<QVBoxLayout*>(ui->scrollAreaWidgetContents->layout());
    scrollLayout->insertWidget(scrollLayout->count() - 1, tokenSection);

    healthReader = std::make_unique<HealthReader>(reader);

    connect(healthReader.get(), &HealthReader::healthDataRead, this, &Health::healthDataReceived);
    connect(healthReader.get(), &HealthReader::certificateDataRead, tokenSection, &TokenSection::setCertificates);
    connect(healthReader.get(), &HealthReader::pinTriesLeftRead, tokenSection, &TokenSection::setPINStatus);
    connect(tokenSection, &TokenSection::changePINRequested, this, &Health::openChangePinDlg);

    connect(healthReader.get(), &HealthReader::readingStarted, [this](){
        printButton->setEnabled(false);
    });
    connect(healthReader.get(), &HealthReader::readingFinished, [this](){
        printButton->setEnabled(true);
    });
    connect(printButton, &QPushButton::clicked, this, &Health::printDocument);

    healthReader->requestData();
    connect(healthReader.get(), &HealthReader::readingFinished, this, [this]() {
        healthReader->requestCertificates();
        healthReader->requestPINTriesLeft();
    }, Qt::SingleShotConnection);
}

Health::~Health()
{
    healthReader.reset();
    delete ui;
}

void Health::healthDataReceived(const healthcard::HealthDocumentData& data)
{
    healthData = data;

    // Personal
    ui->givenNameLineEdit->setText(QString::fromStdString(data.givenName));
    ui->familyNameLineEdit->setText(QString::fromStdString(data.familyName));
    ui->givenNameLatLineEdit->setText(QString::fromStdString(data.givenNameLatin));
    ui->familyNameLatLineEdit->setText(QString::fromStdString(data.familyNameLatin));
    ui->parentNameLineEdit->setText(QString::fromStdString(data.parentName));
    ui->parentNameLatLineEdit->setText(QString::fromStdString(data.parentNameLatin));
    ui->dobLineEdit->setText(QString::fromStdString(data.dateOfBirth));
    ui->genderLineEdit->setText(QString::fromStdString(data.gender));
    ui->jmbgLineEdit->setText(QString::fromStdString(data.personalNumber));
    ui->lboLineEdit->setText(QString::fromStdString(data.insurantNumber));

    // Insurance
    ui->insurerLineEdit->setText(QString::fromStdString(data.insurerName));
    ui->insurerIdLineEdit->setText(QString::fromStdString(data.insurerId));
    ui->cardIdLineEdit->setText(QString::fromStdString(data.cardId));
    ui->issueDateLineEdit->setText(QString::fromStdString(data.dateOfIssue));
    ui->expiryLineEdit->setText(QString::fromStdString(data.dateOfExpiry));
    ui->validUntilLineEdit->setText(QString::fromStdString(data.validUntil));
    ui->permanentLineEdit->setText(data.permanentlyValid ? qtTrId("lc-health-val-yes") : QString());
    ui->insuranceBasisLineEdit->setText(QString::fromStdString(data.insuranceBasisRzzo));
    ui->insuranceDescLineEdit->setText(QString::fromStdString(data.insuranceDescription));
    ui->insuranceStartLineEdit->setText(QString::fromStdString(data.insuranceStartDate));

    // Address
    ui->streetLineEdit->setText(QString::fromStdString(data.street));
    ui->addressNumberLineEdit->setText(QString::fromStdString(data.addressNumber));
    ui->apartmentLineEdit->setText(QString::fromStdString(data.apartment));
    ui->placeLineEdit->setText(QString::fromStdString(data.place));
    ui->municipalityLineEdit->setText(QString::fromStdString(data.municipality));
    ui->countryLineEdit->setText(QString::fromStdString(data.country));

    // Carrier — show section only when insuree is a family member
    ui->carrierSection->setVisible(data.carrierFamilyMember);
    if (data.carrierFamilyMember) {
        ui->carrierGivenNameLineEdit->setText(QString::fromStdString(data.carrierGivenName));
        ui->carrierFamilyNameLineEdit->setText(QString::fromStdString(data.carrierFamilyName));
        ui->carrierRelLineEdit->setText(QString::fromStdString(data.carrierRelationship));
        ui->carrierIdLineEdit->setText(QString::fromStdString(data.carrierIdNumber));
        ui->carrierLboLineEdit->setText(QString::fromStdString(data.carrierInsurantNumber));
    }

    // Taxpayer
    ui->taxpayerNameLineEdit->setText(QString::fromStdString(data.taxpayerName));
    ui->taxpayerIdLineEdit->setText(QString::fromStdString(data.taxpayerIdNumber));
    ui->taxpayerResLineEdit->setText(QString::fromStdString(data.taxpayerResidence));
    ui->taxpayerActLineEdit->setText(QString::fromStdString(data.taxpayerActivityCode));
}

void Health::openChangePinDlg()
{
    auto dlg = std::make_unique<ChangePinDlg>(this);
    connect(dlg.get(), &ChangePinDlg::pinChangeRequested,
            healthReader.get(), &HealthReader::requestChangePIN);
    connect(healthReader.get(), &HealthReader::pinTriesLeftRead,
            dlg.get(), &ChangePinDlg::onPinTriesLeftRead);
    connect(healthReader.get(), &HealthReader::pinChangeSuccess,
            dlg.get(), &ChangePinDlg::onPinChangeSuccess);
    connect(healthReader.get(), &HealthReader::pinChangeFailed,
            dlg.get(), &ChangePinDlg::onPinChangeFailed);
    healthReader->requestPINTriesLeft();
    dlg->exec();
}

void Health::printDocument()
{
    PrintManager::printDocument(HealthTextDocument(healthData), qtTrId("lc-health-print-title"));
}
