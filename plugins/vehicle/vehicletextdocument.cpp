// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QCoreApplication>
#include <QDate>
#include "vehicletextdocument.h"
#include <plugin/carddatautils.h>

using plugin::getFieldValue;

VehicleTextDocument::VehicleTextDocument(const plugin::CardData& cardData, QString documentPath, QString cssPath)
{
    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, cardData);

    setupDocument(data, cssPath);
}

void VehicleTextDocument::translateDocumentData(QString& data) const
{
    data.replace("${title}", qtTrId("lc-vehicle-doc-title"));
    data.replace("${printing_date}", qtTrId("lc-vehicle-doc-printing-date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));
    data.replace("${registration_number}", qtTrId("lc-vehicle-doc-reg-number"));
    data.replace("${issuance_date}", qtTrId("lc-vehicle-doc-issuance-date"));
    data.replace("${validity_date}", qtTrId("lc-vehicle-doc-valid-to"));
    data.replace("${state_issuing}", qtTrId("lc-vehicle-doc-state-issuing"));
    data.replace("${competent_authority}", qtTrId("lc-vehicle-doc-competent-authority"));
    data.replace("${authority_issuing}", qtTrId("lc-vehicle-doc-authority-issuing"));
    data.replace("${unambiguous_number}", qtTrId("lc-vehicle-doc-unambiguous-no"));
    data.replace("${serial_number}", qtTrId("lc-vehicle-doc-serial-no"));
    data.replace("${owner_data}", qtTrId("lc-vehicle-doc-owner-data"));
    data.replace("${owner_surname}", qtTrId("lc-vehicle-doc-owner-surname"));
    data.replace("${owner_name}", qtTrId("lc-vehicle-doc-owner-name"));
    data.replace("${owner_address}", qtTrId("lc-vehicle-doc-owner-address"));
    data.replace("${owner_personal_no}", qtTrId("lc-vehicle-doc-owner-personal-no"));
    data.replace("${user_surname}", qtTrId("lc-vehicle-doc-user-surname"));
    data.replace("${user_name}", qtTrId("lc-vehicle-doc-user-name"));
    data.replace("${user_address}", qtTrId("lc-vehicle-doc-user-address"));
    data.replace("${user_personal_no}", qtTrId("lc-vehicle-doc-user-personal-no"));
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

void VehicleTextDocument::prepareDocumentData(QString& html, const plugin::CardData& cardData) const
{
    html.replace("${registration_number_value}", getPreparedValue(getFieldValue(cardData, "registration_number")));
    html.replace("${issuance_date_value}", getPreparedValue(getFieldValue(cardData, "issuing_date")));
    html.replace("${validity_date_value}", getPreparedValue(getFieldValue(cardData, "expiry_date")));
    html.replace("${state_issuing_value}", getPreparedValue(getFieldValue(cardData, "state_issuing")));
    html.replace("${competent_authority_value}", getPreparedValue(getFieldValue(cardData, "competent_authority")));
    html.replace("${authority_issuing_value}", getPreparedValue(getFieldValue(cardData, "authority_issuing")));
    html.replace("${unambiguous_number_value}", getPreparedValue(getFieldValue(cardData, "unambiguous_number")));
    html.replace("${serial_number_value}", getPreparedValue(getFieldValue(cardData, "serial_number")));
    html.replace("${owner_surname_value}",
                 getPreparedValue(getFieldValue(cardData, "owners_surname_or_business_name")));
    html.replace("${owner_name_value}", getPreparedValue(getFieldValue(cardData, "owner_name")));
    html.replace("${owner_address_value}", getPreparedValue(getFieldValue(cardData, "owner_address")));
    html.replace("${owner_personal_no_value}", getPreparedValue(getFieldValue(cardData, "owners_personal_no")));
    html.replace("${user_surname_value}",
                 getPreparedValue(getFieldValue(cardData, "users_surname_or_business_name")));
    html.replace("${user_name_value}", getPreparedValue(getFieldValue(cardData, "users_name")));
    html.replace("${user_address_value}", getPreparedValue(getFieldValue(cardData, "users_address")));
    html.replace("${user_personal_no_value}", getPreparedValue(getFieldValue(cardData, "users_personal_no")));
    html.replace("${date_of_first_registration_value}",
                 getPreparedValue(getFieldValue(cardData, "date_of_first_registration")));
    html.replace("${year_of_production_value}", getPreparedValue(getFieldValue(cardData, "year_of_production")));
    html.replace("${make_value}", getPreparedValue(getFieldValue(cardData, "vehicle_make")));
    html.replace("${type_value}", getPreparedValue(getFieldValue(cardData, "vehicle_type")));
    html.replace("${commercial_description_value}",
                 getPreparedValue(getFieldValue(cardData, "commercial_description")));
    html.replace("${type_approval_number_value}", getPreparedValue(getFieldValue(cardData, "type_approval_number")));
    html.replace("${colour_value}", getPreparedValue(getFieldValue(cardData, "colour_of_vehicle")));
    html.replace("${number_of_axles_value}", getPreparedValue(getFieldValue(cardData, "number_of_axes")));
    html.replace("${vin_value}", getPreparedValue(getFieldValue(cardData, "vehicle_id_number")));
    html.replace("${capacity_value}", getPreparedValue(getFieldValue(cardData, "engine_capacity")));
    html.replace("${engine_number_value}", getPreparedValue(getFieldValue(cardData, "engine_id_number")));
    html.replace("${mass_value}", getPreparedValue(getFieldValue(cardData, "vehicle_mass")));
    html.replace("${power_value}", getPreparedValue(getFieldValue(cardData, "maximum_net_power")));
    html.replace("${load_value}", getPreparedValue(getFieldValue(cardData, "vehicle_load")));
    html.replace("${power_weight_ratio_value}", getPreparedValue(getFieldValue(cardData, "power_weight_ratio")));
    html.replace("${max_laden_mass_value}",
                 getPreparedValue(getFieldValue(cardData, "maximum_permissible_laden_mass")));
    html.replace("${category_value}", getPreparedValue(getFieldValue(cardData, "vehicle_category")));
    html.replace("${fuel_type_value}", getPreparedValue(getFieldValue(cardData, "type_of_fuel")));
    html.replace("${seats_value}", getPreparedValue(getFieldValue(cardData, "number_of_seats")));
    html.replace("${standing_places_value}", getPreparedValue(getFieldValue(cardData, "number_of_standing_places")));
}
