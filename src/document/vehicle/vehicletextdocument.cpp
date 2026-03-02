// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QCoreApplication>
#include <QDate>
#include "vehicletextdocument.h"

VehicleTextDocument::VehicleTextDocument(const vehiclecard::VehicleDocumentData& vehicleData,
                                         QString documentPath,
                                         QString cssPath)
{
    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, vehicleData);

    setupDocument(data, cssPath);
}

void VehicleTextDocument::translateDocumentData(QString& data) const
{
    data.replace("${title}", qtTrId("lc-vehicle-doc-title"));
    data.replace("${printing_date}", qtTrId("lc-vehicle-doc-printing-date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));

    // Registration number header
    data.replace("${registration_number}", qtTrId("lc-vehicle-doc-reg-number"));

    // Document data section (no heading)
    data.replace("${issuance_date}", qtTrId("lc-vehicle-doc-issuance-date"));
    data.replace("${validity_date}", qtTrId("lc-vehicle-doc-valid-to"));
    data.replace("${state_issuing}", qtTrId("lc-vehicle-doc-state-issuing"));
    data.replace("${competent_authority}", qtTrId("lc-vehicle-doc-competent-authority"));
    data.replace("${authority_issuing}", qtTrId("lc-vehicle-doc-authority-issuing"));
    data.replace("${unambiguous_number}", qtTrId("lc-vehicle-doc-unambiguous-no"));
    data.replace("${serial_number}", qtTrId("lc-vehicle-doc-serial-no"));

    // Owner data section (combined owner + user)
    data.replace("${owner_data}", qtTrId("lc-vehicle-doc-owner-data"));
    data.replace("${owner_surname}", qtTrId("lc-vehicle-doc-owner-surname"));
    data.replace("${owner_name}", qtTrId("lc-vehicle-doc-owner-name"));
    data.replace("${owner_address}", qtTrId("lc-vehicle-doc-owner-address"));
    data.replace("${owner_personal_no}", qtTrId("lc-vehicle-doc-owner-personal-no"));
    data.replace("${user_surname}", qtTrId("lc-vehicle-doc-user-surname"));
    data.replace("${user_name}", qtTrId("lc-vehicle-doc-user-name"));
    data.replace("${user_address}", qtTrId("lc-vehicle-doc-user-address"));
    data.replace("${user_personal_no}", qtTrId("lc-vehicle-doc-user-personal-no"));

    // Vehicle data section (combined vehicle + engine + mass)
    data.replace("${vehicle_data}", qtTrId("lc-vehicle-doc-vehicle-data"));
    data.replace("${date_of_first_registration}", qtTrId("lc-vehicle-doc-first-reg-date"));
    data.replace("${year_of_production}", qtTrId("lc-vehicle-doc-production-year"));
    data.replace("${make}", qtTrId("lc-vehicle-doc-make"));
    data.replace("${type}", qtTrId("lc-vehicle-doc-type"));
    data.replace("${commercial_description}", qtTrId("lc-vehicle-doc-commercial-desc"));
    data.replace("${type_approval_number}", qtTrId("lc-vehicle-doc-type-approval-no"));
    data.replace("${colour}", qtTrId("lc-vehicle-doc-colour"));
    data.replace("${number_of_axles}", qtTrId("lc-vehicle-doc-axles"));
    data.replace("${vin}", qtTrId("lc-vehicle-doc-vin"));
    data.replace("${capacity}", qtTrId("lc-vehicle-doc-capacity"));
    data.replace("${engine_number}", qtTrId("lc-vehicle-doc-engine-number"));
    data.replace("${mass}", qtTrId("lc-vehicle-doc-mass"));
    data.replace("${power}", qtTrId("lc-vehicle-doc-power"));
    data.replace("${load}", qtTrId("lc-vehicle-doc-load"));
    data.replace("${power_weight_ratio}", qtTrId("lc-vehicle-doc-power-weight"));
    data.replace("${max_laden_mass}", qtTrId("lc-vehicle-doc-max-laden-mass"));
    data.replace("${category}", qtTrId("lc-vehicle-doc-category"));
    data.replace("${fuel_type}", qtTrId("lc-vehicle-doc-fuel-type"));
    data.replace("${seats}", qtTrId("lc-vehicle-doc-seats"));
    data.replace("${standing_places}", qtTrId("lc-vehicle-doc-standing-places"));
}

void VehicleTextDocument::prepareDocumentData(QString& data, const vehiclecard::VehicleDocumentData& vehicleData) const
{
    auto s = [](const std::string& v) { return QString::fromStdString(v); };

    data.replace("${registration_number_value}", getPreparedValue(s(vehicleData.registrationNumber)));

    // Document data values
    data.replace("${issuance_date_value}", getPreparedValue(s(vehicleData.issuingDate)));
    data.replace("${validity_date_value}", getPreparedValue(s(vehicleData.expiryDate)));
    data.replace("${state_issuing_value}", getPreparedValue(s(vehicleData.stateIssuing)));
    data.replace("${competent_authority_value}", getPreparedValue(s(vehicleData.competentAuthority)));
    data.replace("${authority_issuing_value}", getPreparedValue(s(vehicleData.authorityIssuing)));
    data.replace("${unambiguous_number_value}", getPreparedValue(s(vehicleData.unambiguousNumber)));
    data.replace("${serial_number_value}", getPreparedValue(s(vehicleData.serialNumber)));

    // Owner + User values
    data.replace("${owner_surname_value}", getPreparedValue(s(vehicleData.ownersSurnameOrBusinessName)));
    data.replace("${owner_name_value}", getPreparedValue(s(vehicleData.ownerName)));
    data.replace("${owner_address_value}", getPreparedValue(s(vehicleData.ownerAddress)));
    data.replace("${owner_personal_no_value}", getPreparedValue(s(vehicleData.ownersPersonalNo)));
    data.replace("${user_surname_value}", getPreparedValue(s(vehicleData.usersSurnameOrBusinessName)));
    data.replace("${user_name_value}", getPreparedValue(s(vehicleData.usersName)));
    data.replace("${user_address_value}", getPreparedValue(s(vehicleData.usersAddress)));
    data.replace("${user_personal_no_value}", getPreparedValue(s(vehicleData.usersPersonalNo)));

    // Vehicle data values
    data.replace("${date_of_first_registration_value}", getPreparedValue(s(vehicleData.dateOfFirstRegistration)));
    data.replace("${year_of_production_value}", getPreparedValue(s(vehicleData.yearOfProduction)));
    data.replace("${make_value}", getPreparedValue(s(vehicleData.vehicleMake)));
    data.replace("${type_value}", getPreparedValue(s(vehicleData.vehicleType)));
    data.replace("${commercial_description_value}", getPreparedValue(s(vehicleData.commercialDescription)));
    data.replace("${type_approval_number_value}", getPreparedValue(s(vehicleData.typeApprovalNumber)));
    data.replace("${colour_value}", getPreparedValue(s(vehicleData.colourOfVehicle)));
    data.replace("${number_of_axles_value}", getPreparedValue(s(vehicleData.numberOfAxles)));
    data.replace("${vin_value}", getPreparedValue(s(vehicleData.vehicleIdNumber)));
    data.replace("${capacity_value}", getPreparedValue(s(vehicleData.engineCapacity)));
    data.replace("${engine_number_value}", getPreparedValue(s(vehicleData.engineIdNumber)));
    data.replace("${mass_value}", getPreparedValue(s(vehicleData.vehicleMass)));
    data.replace("${power_value}", getPreparedValue(s(vehicleData.maximumNetPower)));
    data.replace("${load_value}", getPreparedValue(s(vehicleData.vehicleLoad)));
    data.replace("${power_weight_ratio_value}", getPreparedValue(s(vehicleData.powerWeightRatio)));
    data.replace("${max_laden_mass_value}", getPreparedValue(s(vehicleData.maximumPermissibleLadenMass)));
    data.replace("${category_value}", getPreparedValue(s(vehicleData.vehicleCategory)));
    data.replace("${fuel_type_value}", getPreparedValue(s(vehicleData.typeOfFuel)));
    data.replace("${seats_value}", getPreparedValue(s(vehicleData.numberOfSeats)));
    data.replace("${standing_places_value}", getPreparedValue(s(vehicleData.numberOfStandingPlaces)));
}
