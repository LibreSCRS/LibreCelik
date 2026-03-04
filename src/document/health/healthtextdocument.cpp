// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QCoreApplication>
#include <QDate>
#include "healthtextdocument.h"

HealthTextDocument::HealthTextDocument(const healthcard::HealthDocumentData& healthData,
                                       QString documentPath,
                                       QString cssPath)
{
    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, healthData);

    setupDocument(data, cssPath);
}

void HealthTextDocument::translateDocumentData(QString& data) const
{
    data.replace("${title}", qtTrId("lc-health-doc-title"));
    data.replace("${printing_date}", qtTrId("lc-health-doc-printing-date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));

    // Personal
    data.replace("${given_name}", qtTrId("lc-health-label-given-name"));
    data.replace("${family_name}", qtTrId("lc-health-label-family-name"));
    data.replace("${parent_name}", qtTrId("lc-health-label-parent-name"));
    data.replace("${dob}", qtTrId("lc-health-label-dob"));
    data.replace("${gender}", qtTrId("lc-health-label-gender"));
    data.replace("${jmbg}", qtTrId("lc-health-label-jmbg"));
    data.replace("${lbo}", qtTrId("lc-health-label-lbo"));

    // Insurance
    data.replace("${section_personal}", qtTrId("lc-health-section-personal"));
    data.replace("${section_insurance}", qtTrId("lc-health-section-insurance"));
    data.replace("${section_address}", qtTrId("lc-health-section-address"));
    data.replace("${section_taxpayer}", qtTrId("lc-health-section-taxpayer"));
    data.replace("${insurer}", qtTrId("lc-health-label-insurer"));
    data.replace("${insurer_id}", qtTrId("lc-health-label-insurer-id"));
    data.replace("${card_id}", qtTrId("lc-health-label-card-id"));
    data.replace("${issue_date}", qtTrId("lc-health-label-issue-date"));
    data.replace("${expiry}", qtTrId("lc-health-label-expiry"));
    data.replace("${valid_until}", qtTrId("lc-health-label-valid-until"));
    data.replace("${insurance_basis}", qtTrId("lc-health-label-insurance-basis"));
    data.replace("${insurance_desc}", qtTrId("lc-health-label-insurance-desc"));
    data.replace("${insurance_start}", qtTrId("lc-health-label-insurance-start"));

    // Address
    data.replace("${street}", qtTrId("lc-health-label-street"));
    data.replace("${address_number}", qtTrId("lc-health-label-number"));
    data.replace("${apartment}", qtTrId("lc-health-label-apartment"));
    data.replace("${place}", qtTrId("lc-health-label-place"));
    data.replace("${municipality}", qtTrId("lc-health-label-municipality"));
    data.replace("${country}", qtTrId("lc-health-label-country"));

    // Taxpayer
    data.replace("${taxpayer_name}", qtTrId("lc-health-label-taxpayer-name"));
    data.replace("${taxpayer_id}", qtTrId("lc-health-label-taxpayer-id"));
    data.replace("${taxpayer_res}", qtTrId("lc-health-label-taxpayer-res"));
    data.replace("${taxpayer_act}", qtTrId("lc-health-label-taxpayer-act"));
}

void HealthTextDocument::prepareDocumentData(QString& data, const healthcard::HealthDocumentData& healthData) const
{
    auto s = [](const std::string& v) { return QString::fromStdString(v); };

    // Personal
    data.replace("${given_name_value}", getPreparedValue(s(healthData.givenName)));
    data.replace("${family_name_value}", getPreparedValue(s(healthData.familyName)));
    data.replace("${given_name_lat_value}", getPreparedValue(s(healthData.givenNameLatin)));
    data.replace("${family_name_lat_value}", getPreparedValue(s(healthData.familyNameLatin)));
    data.replace("${parent_name_value}", getPreparedValue(s(healthData.parentName)));
    data.replace("${dob_value}", getPreparedValue(s(healthData.dateOfBirth)));
    data.replace("${gender_value}", getPreparedValue(s(healthData.gender)));
    data.replace("${jmbg_value}", getPreparedValue(s(healthData.personalNumber)));
    data.replace("${lbo_value}", getPreparedValue(s(healthData.insurantNumber)));

    // Insurance
    data.replace("${insurer_value}", getPreparedValue(s(healthData.insurerName)));
    data.replace("${insurer_id_value}", getPreparedValue(s(healthData.insurerId)));
    data.replace("${card_id_value}", getPreparedValue(s(healthData.cardId)));
    data.replace("${issue_date_value}", getPreparedValue(s(healthData.dateOfIssue)));
    data.replace("${expiry_value}", getPreparedValue(s(healthData.dateOfExpiry)));
    data.replace("${valid_until_value}", getPreparedValue(s(healthData.validUntil)));
    data.replace("${insurance_basis_value}", getPreparedValue(s(healthData.insuranceBasisRzzo)));
    data.replace("${insurance_desc_value}", getPreparedValue(s(healthData.insuranceDescription)));
    data.replace("${insurance_start_value}", getPreparedValue(s(healthData.insuranceStartDate)));

    // Address
    data.replace("${street_value}", getPreparedValue(s(healthData.street)));
    data.replace("${address_number_value}", getPreparedValue(s(healthData.addressNumber)));
    data.replace("${apartment_value}", getPreparedValue(s(healthData.apartment)));
    data.replace("${place_value}", getPreparedValue(s(healthData.place)));
    data.replace("${municipality_value}", getPreparedValue(s(healthData.municipality)));
    data.replace("${country_value}", getPreparedValue(s(healthData.country)));

    // Taxpayer
    data.replace("${taxpayer_name_value}", getPreparedValue(s(healthData.taxpayerName)));
    data.replace("${taxpayer_id_value}", getPreparedValue(s(healthData.taxpayerIdNumber)));
    data.replace("${taxpayer_res_value}", getPreparedValue(s(healthData.taxpayerResidence)));
    data.replace("${taxpayer_act_value}", getPreparedValue(s(healthData.taxpayerActivityCode)));
}
