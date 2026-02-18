// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QDate>
#include "utils/libreceliklog.h"
#include "utils/printmanager.h"
#include "vehicle.h"
#include "vehicletextdocument.h"
#include "vehiclereader.h"
#include "ui_vehicle.h"

Vehicle::Vehicle(std::string reader, QWidget *parent)
    : Document(parent)
    , ui(new Ui::Vehicle)
{
    ui->setupUi(this);

    vehicleReader = std::make_unique<VehicleReader>(reader);

    connect(vehicleReader.get(), &VehicleReader::vehicleDataRead, this, &Vehicle::vehicleDataReceived);

    connect(vehicleReader.get(), &VehicleReader::readingStarted, [this](){
        ui->toolButton->setEnabled(false);
    });
    connect(vehicleReader.get(), &VehicleReader::readingFinished, [this](){
        ui->toolButton->setEnabled(true);
    });

    vehicleReader->requestData();
}

Vehicle::~Vehicle()
{
    vehicleReader.reset();
    delete ui;
}

void Vehicle::vehicleDataReceived(const vehiclecard::VehicleDocumentData& data)
{
    vehicleData = data;

    // Registration
    ui->registrationNumberLineEdit->setText(QString::fromStdString(data.registrationNumber));
    ui->dateOfFirstRegistrationLineEdit->setText(QString::fromStdString(data.dateOfFirstRegistration));

    // Vehicle
    ui->vehicleIdNumberLineEdit->setText(QString::fromStdString(data.vehicleIdNumber));
    ui->vehicleMakeLineEdit->setText(QString::fromStdString(data.vehicleMake));
    ui->vehicleTypeLineEdit->setText(QString::fromStdString(data.vehicleType));
    ui->commercialDescriptionLineEdit->setText(QString::fromStdString(data.commercialDescription));
    ui->vehicleCategoryLineEdit->setText(QString::fromStdString(data.vehicleCategory));
    ui->colourOfVehicleLineEdit->setText(QString::fromStdString(data.colourOfVehicle));
    ui->yearOfProductionLineEdit->setText(QString::fromStdString(data.yearOfProduction));

    // Engine
    ui->engineIdNumberLineEdit->setText(QString::fromStdString(data.engineIdNumber));
    ui->engineCapacityLineEdit->setText(QString::fromStdString(data.engineCapacity));
    ui->maximumNetPowerLineEdit->setText(QString::fromStdString(data.maximumNetPower));
    ui->typeOfFuelLineEdit->setText(QString::fromStdString(data.typeOfFuel));

    // Mass
    ui->vehicleMassLineEdit->setText(QString::fromStdString(data.vehicleMass));
    ui->maximumPermissibleLadenMassLineEdit->setText(QString::fromStdString(data.maximumPermissibleLadenMass));
    ui->vehicleLoadLineEdit->setText(QString::fromStdString(data.vehicleLoad));
    ui->powerWeightRatioLineEdit->setText(QString::fromStdString(data.powerWeightRatio));
    ui->numberOfAxlesLineEdit->setText(QString::fromStdString(data.numberOfAxles));

    // Capacity
    ui->numberOfSeatsLineEdit->setText(QString::fromStdString(data.numberOfSeats));
    ui->numberOfStandingPlacesLineEdit->setText(QString::fromStdString(data.numberOfStandingPlaces));

    // Document
    auto expiryDate = QString::fromStdString(data.expiryDate);
    QDate receivedDate = QDate::fromString(expiryDate, "dd.MM.yyyy");
    QDate currentDate = QDate::currentDate();
    QPalette palette = ui->expiryDateLineEdit->palette();
    if (receivedDate.isValid() && receivedDate < currentDate)
    {
        palette.setColor(QPalette::Text, QColor(0xe6, 0x87, 0x3c));
    }
    ui->expiryDateLineEdit->setPalette(palette);

    ui->expiryDateLineEdit->setText(expiryDate);
    ui->issuingDateLineEdit->setText(QString::fromStdString(data.issuingDate));
    ui->typeApprovalNumberLineEdit->setText(QString::fromStdString(data.typeApprovalNumber));
    ui->stateIssuingLineEdit->setText(QString::fromStdString(data.stateIssuing));
    ui->competentAuthorityLineEdit->setText(QString::fromStdString(data.competentAuthority));
    ui->authorityIssuingLineEdit->setText(QString::fromStdString(data.authorityIssuing));
    ui->unambiguousNumberLineEdit->setText(QString::fromStdString(data.unambiguousNumber));
    ui->serialNumberLineEdit->setText(QString::fromStdString(data.serialNumber));

    // Owner
    ui->ownersSurnameLineEdit->setText(QString::fromStdString(data.ownersSurnameOrBusinessName));
    ui->ownerNameLineEdit->setText(QString::fromStdString(data.ownerName));
    ui->ownerAddressLineEdit->setText(QString::fromStdString(data.ownerAddress));
    ui->ownersPersonalNoLineEdit->setText(QString::fromStdString(data.ownersPersonalNo));

    // User
    ui->usersSurnameLineEdit->setText(QString::fromStdString(data.usersSurnameOrBusinessName));
    ui->usersNameLineEdit->setText(QString::fromStdString(data.usersName));
    ui->usersAddressLineEdit->setText(QString::fromStdString(data.usersAddress));
    ui->usersPersonalNoLineEdit->setText(QString::fromStdString(data.usersPersonalNo));
}

void Vehicle::on_toolButton_clicked()
{
    PrintManager::printDocument(VehicleTextDocument(vehicleData), tr("Print Document"));
}
