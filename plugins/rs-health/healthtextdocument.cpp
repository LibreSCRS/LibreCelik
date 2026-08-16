// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "healthtextdocument.h"
#include <plugin/fieldvalue.h>

using librecelik::plugin::fieldValue;
using LibreSCRS::AgentClient::FieldGroup;

HealthTextDocument::HealthTextDocument(const QList<FieldGroup>& groups, QString documentPath, QString cssPath)
{
    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, groups);

    setupDocument(data, cssPath);
}

void HealthTextDocument::translateDocumentData(QString& data) const
{
    const auto title =
        qtTrId("lc-health-doc-title"); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
    data.replace("${title}", title);
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

void HealthTextDocument::prepareDocumentData(QString& html, const QList<FieldGroup>& groups) const
{
    html.replace("${given_name_value}", getPreparedValue(fieldValue(groups, u"given_name")));
    html.replace("${family_name_value}", getPreparedValue(fieldValue(groups, u"family_name")));
    html.replace("${given_name_lat_value}", getPreparedValue(fieldValue(groups, u"given_name_latin")));
    html.replace("${family_name_lat_value}", getPreparedValue(fieldValue(groups, u"family_name_latin")));
    html.replace("${parent_name_value}", getPreparedValue(fieldValue(groups, u"parent_name")));
    html.replace("${dob_value}", getPreparedValue(fieldValue(groups, u"date_of_birth")));
    html.replace("${gender_value}", getPreparedValue(fieldValue(groups, u"gender")));
    html.replace("${jmbg_value}", getPreparedValue(fieldValue(groups, u"personal_number")));
    html.replace("${lbo_value}", getPreparedValue(fieldValue(groups, u"insurant_number")));
    html.replace("${insurer_value}", getPreparedValue(fieldValue(groups, u"insurer_name")));
    html.replace("${insurer_id_value}", getPreparedValue(fieldValue(groups, u"insurer_id")));
    html.replace("${card_id_value}", getPreparedValue(fieldValue(groups, u"card_id")));
    html.replace("${issue_date_value}", getPreparedValue(fieldValue(groups, u"date_of_issue")));
    html.replace("${expiry_value}", getPreparedValue(fieldValue(groups, u"date_of_expiry")));
    html.replace("${valid_until_value}", getPreparedValue(fieldValue(groups, u"valid_until")));
    html.replace("${insurance_basis_value}", getPreparedValue(fieldValue(groups, u"insurance_basis_rzzo")));
    html.replace("${insurance_desc_value}", getPreparedValue(fieldValue(groups, u"insurance_description")));
    html.replace("${insurance_start_value}", getPreparedValue(fieldValue(groups, u"insurance_start_date")));
    html.replace("${street_value}", getPreparedValue(fieldValue(groups, u"street")));
    html.replace("${address_number_value}", getPreparedValue(fieldValue(groups, u"address_number")));
    html.replace("${apartment_value}", getPreparedValue(fieldValue(groups, u"apartment")));
    html.replace("${place_value}", getPreparedValue(fieldValue(groups, u"place")));
    html.replace("${municipality_value}", getPreparedValue(fieldValue(groups, u"municipality")));
    html.replace("${country_value}", getPreparedValue(fieldValue(groups, u"country")));
    html.replace("${taxpayer_name_value}", getPreparedValue(fieldValue(groups, u"taxpayer_name")));
    html.replace("${taxpayer_id_value}", getPreparedValue(fieldValue(groups, u"taxpayer_id_number")));
    html.replace("${taxpayer_res_value}", getPreparedValue(fieldValue(groups, u"taxpayer_residence")));
    html.replace("${taxpayer_act_value}", getPreparedValue(fieldValue(groups, u"taxpayer_activity_code")));
}
