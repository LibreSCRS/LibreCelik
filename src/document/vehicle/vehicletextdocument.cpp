// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

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
    data.replace("${title}", tr("Vehicle registration card reader"));
    data.replace("${printing_date}", tr("Printing date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));

    // Registration number header
    data.replace("${registration_number}", QCoreApplication::translate("Vehicle", "Registration number"));

    // Document data section (no heading)
    data.replace("${issuance_date}", QCoreApplication::translate("Vehicle", "Date of issuance"));
    data.replace("${validity_date}", QCoreApplication::translate("Vehicle", "Valid to"));
    data.replace("${state_issuing}", QCoreApplication::translate("Vehicle", "State issuing"));
    data.replace("${competent_authority}", QCoreApplication::translate("Vehicle", "Competent authority"));
    data.replace("${authority_issuing}", QCoreApplication::translate("Vehicle", "Authority issuing"));
    data.replace("${unambiguous_number}", QCoreApplication::translate("Vehicle", "Unambiguous number"));
    data.replace("${serial_number}", QCoreApplication::translate("Vehicle", "Serial number"));

    // Owner data section (combined owner + user)
    data.replace("${owner_data}", tr("Owner data"));
    data.replace("${owner_surname}", QCoreApplication::translate("Vehicle", "Surname / Business name"));
    data.replace("${owner_name}", QCoreApplication::translate("Vehicle", "Name"));
    data.replace("${owner_address}", QCoreApplication::translate("Vehicle", "Address"));
    data.replace("${owner_personal_no}", QCoreApplication::translate("Vehicle", "Personal number"));
    data.replace("${user_surname}", QCoreApplication::translate("Vehicle", "Surname / Business name"));
    data.replace("${user_name}", QCoreApplication::translate("Vehicle", "Name"));
    data.replace("${user_address}", QCoreApplication::translate("Vehicle", "Address"));
    data.replace("${user_personal_no}", QCoreApplication::translate("Vehicle", "Personal number"));

    // Vehicle data section (combined vehicle + engine + mass)
    data.replace("${vehicle_data}", QCoreApplication::translate("Vehicle", "Vehicle data"));
    data.replace("${date_of_first_registration}", QCoreApplication::translate("Vehicle", "Date of first registration"));
    data.replace("${year_of_production}", QCoreApplication::translate("Vehicle", "Year of production"));
    data.replace("${make}", QCoreApplication::translate("Vehicle", "Make"));
    data.replace("${type}", QCoreApplication::translate("Vehicle", "Type"));
    data.replace("${commercial_description}", QCoreApplication::translate("Vehicle", "Commercial description"));
    data.replace("${type_approval_number}", QCoreApplication::translate("Vehicle", "Type approval number"));
    data.replace("${colour}", QCoreApplication::translate("Vehicle", "Colour"));
    data.replace("${number_of_axles}", QCoreApplication::translate("Vehicle", "Number of axles"));
    data.replace("${vin}", QCoreApplication::translate("Vehicle", "VIN"));
    data.replace("${capacity}", QCoreApplication::translate("Vehicle", "Capacity (cm3)"));
    data.replace("${engine_number}", QCoreApplication::translate("Vehicle", "Engine number"));
    data.replace("${mass}", QCoreApplication::translate("Vehicle", "Mass (kg)"));
    data.replace("${power}", QCoreApplication::translate("Vehicle", "Power (kW)"));
    data.replace("${load}", QCoreApplication::translate("Vehicle", "Load (kg)"));
    data.replace("${power_weight_ratio}", QCoreApplication::translate("Vehicle", "Power/weight ratio"));
    data.replace("${max_laden_mass}", QCoreApplication::translate("Vehicle", "Max laden mass (kg)"));
    data.replace("${category}", QCoreApplication::translate("Vehicle", "Category"));
    data.replace("${fuel_type}", QCoreApplication::translate("Vehicle", "Fuel type"));
    data.replace("${seats}", QCoreApplication::translate("Vehicle", "Seats"));
    data.replace("${standing_places}", QCoreApplication::translate("Vehicle", "Standing places"));
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
