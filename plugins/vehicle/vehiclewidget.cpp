// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "vehiclewidget.h"
#include "ui_vehicle.h"

#include <plugin/carddatautils.h>

#include <QDate>

using plugin::getFieldValue;

VehicleWidget::VehicleWidget(const plugin::CardData& data, QWidget* parent)
    : QWidget(parent), ui(new Ui::Vehicle), data(data)
{
    ui->setupUi(this);
    ui->vehicleCardSection->setTitle(qtTrId("lc-vehicle-title"));
    ui->verticalLayout->setStretch(0, 1);
    ui->vehicleCardSection->setHeaderHeight(56);

    // Populate data from groups — all vehicle fields are in a single "vehicle" group
    if (const auto* group = data.findGroup("vehicle")) {
        populateVehicleData(group);
    }

    // Owner fields
    if (const auto* group = data.findGroup("owner")) {
        populateOwnerData(group);
    } else if (const auto* group = data.findGroup("vehicle")) {
        populateOwnerData(group);
    }

    // User fields
    if (const auto* group = data.findGroup("user")) {
        populateUserData(group);
    } else if (const auto* group = data.findGroup("vehicle")) {
        populateUserData(group);
    }
}

VehicleWidget::~VehicleWidget()
{
    delete ui;
}

void VehicleWidget::populateVehicleData(const plugin::CardFieldGroup* group)
{
    // Registration
    ui->registrationNumberLineEdit->setText(getFieldValue(group, "registration_number"));
    ui->dateOfFirstRegistrationLineEdit->setText(getFieldValue(group, "date_of_first_registration"));

    // Vehicle
    ui->vehicleIdNumberLineEdit->setText(getFieldValue(group, "vehicle_id_number"));
    ui->vehicleMakeLineEdit->setText(getFieldValue(group, "vehicle_make"));
    ui->vehicleTypeLineEdit->setText(getFieldValue(group, "vehicle_type"));
    ui->commercialDescriptionLineEdit->setText(getFieldValue(group, "commercial_description"));
    ui->vehicleCategoryLineEdit->setText(getFieldValue(group, "vehicle_category"));
    ui->colourOfVehicleLineEdit->setText(getFieldValue(group, "colour_of_vehicle"));
    ui->yearOfProductionLineEdit->setText(getFieldValue(group, "year_of_production"));

    // Engine
    ui->engineIdNumberLineEdit->setText(getFieldValue(group, "engine_id_number"));
    ui->engineCapacityLineEdit->setText(getFieldValue(group, "engine_capacity"));
    ui->maximumNetPowerLineEdit->setText(getFieldValue(group, "maximum_net_power"));
    ui->typeOfFuelLineEdit->setText(getFieldValue(group, "type_of_fuel"));

    // Mass
    ui->vehicleMassLineEdit->setText(getFieldValue(group, "vehicle_mass"));
    ui->maximumPermissibleLadenMassLineEdit->setText(getFieldValue(group, "maximum_permissible_laden_mass"));
    ui->vehicleLoadLineEdit->setText(getFieldValue(group, "vehicle_load"));
    ui->powerWeightRatioLineEdit->setText(getFieldValue(group, "power_weight_ratio"));
    ui->numberOfAxlesLineEdit->setText(getFieldValue(group, "number_of_axes"));

    // Capacity
    ui->numberOfSeatsLineEdit->setText(getFieldValue(group, "number_of_seats"));
    ui->numberOfStandingPlacesLineEdit->setText(getFieldValue(group, "number_of_standing_places"));

    // Document — expiry date with orange highlight if expired
    auto expiryDate = getFieldValue(group, "expiry_date");
    QDate receivedDate = QDate::fromString(expiryDate, "dd.MM.yyyy");
    QDate currentDate = QDate::currentDate();
    QPalette palette = ui->expiryDateLineEdit->palette();
    if (receivedDate.isValid() && receivedDate < currentDate) {
        palette.setColor(QPalette::Text, QColor(0xe6, 0x87, 0x3c));
    }
    ui->expiryDateLineEdit->setPalette(palette);
    ui->expiryDateLineEdit->setText(expiryDate);

    ui->issuingDateLineEdit->setText(getFieldValue(group, "issuing_date"));
    ui->typeApprovalNumberLineEdit->setText(getFieldValue(group, "type_approval_number"));
    ui->stateIssuingLineEdit->setText(getFieldValue(group, "state_issuing"));
    ui->competentAuthorityLineEdit->setText(getFieldValue(group, "competent_authority"));
    ui->authorityIssuingLineEdit->setText(getFieldValue(group, "authority_issuing"));
    ui->unambiguousNumberLineEdit->setText(getFieldValue(group, "unambiguous_number"));
    ui->serialNumberLineEdit->setText(getFieldValue(group, "serial_number"));
}

void VehicleWidget::populateOwnerData(const plugin::CardFieldGroup* group)
{
    ui->ownersSurnameLineEdit->setText(getFieldValue(group, "owners_surname_or_business_name"));
    ui->ownerNameLineEdit->setText(getFieldValue(group, "owner_name"));
    ui->ownerAddressLineEdit->setText(getFieldValue(group, "owner_address"));
    ui->ownersPersonalNoLineEdit->setText(getFieldValue(group, "owners_personal_no"));
}

void VehicleWidget::populateUserData(const plugin::CardFieldGroup* group)
{
    ui->usersSurnameLineEdit->setText(getFieldValue(group, "users_surname_or_business_name"));
    ui->usersNameLineEdit->setText(getFieldValue(group, "users_name"));
    ui->usersAddressLineEdit->setText(getFieldValue(group, "users_address"));
    ui->usersPersonalNoLineEdit->setText(getFieldValue(group, "users_personal_no"));

    // Hide User section when all fields are empty
    auto surname = getFieldValue(group, "users_surname_or_business_name");
    auto name = getFieldValue(group, "users_name");
    auto address = getFieldValue(group, "users_address");
    auto personalNo = getFieldValue(group, "users_personal_no");
    if (surname.isEmpty() && name.isEmpty() && address.isEmpty() && personalNo.isEmpty()) {
        ui->userGroupBox->setVisible(false);
    }
}
