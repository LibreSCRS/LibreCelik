// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "euvrctextdocument.h"
#include "utils/stringutils.h"
#include <plugin/fieldvalue.h>

using librecelik::plugin::fieldValue;
using librecelik::plugin::findGroup;
using FieldGroupList = QList<LibreSCRS::AgentClient::FieldGroup>;

EuVrcTextDocument::EuVrcTextDocument(const FieldGroupList& groups, QString cssPath)
{
    auto html = buildHtml(groups);
    setupDocument(html, cssPath);
}

QString EuVrcTextDocument::emitRow(const QString& label, const QString& value, const QString& cssClass) const
{
    if (value.isEmpty() || value == "01.01.0001")
        return {};

    QString cls = cssClass.isEmpty() ? QString() : QString(" class=\"%1\"").arg(cssClass);
    return QString("<tr>"
                   "<td width=\"0\"><img src=\":/images/transparent_1x20.png\" width=\"1\" height=\"8\"></td>"
                   "<td width=\"25%%\"><b>%1:</b></td>"
                   "<td%3>%2</td>"
                   "</tr>\n")
        .arg(label.toHtmlEscaped(), value.toHtmlEscaped(), cls);
}

QString EuVrcTextDocument::buildHtml(const FieldGroupList& groups) const
{
    QString html;
    html +=
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<html xmlns=\"http://www.w3.org/1999/xhtml\" xml:lang=\"en\" lang=\"en\">\n"
        "<head><title>" +
        qtTrId("lc-euvrc-doc-title") + // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
        "</title>\n"
        "<link rel=\"stylesheet\" type=\"text/css\" href=\":/html/euvrccard.css\" title=\"Style\"/>\n"
        "</head>\n<body>\n";

    html += "<h1>" + qtTrId("lc-euvrc-doc-title") + "</h1>\n";

    // Registration number as h2 header
    auto regNum = fieldValue(groups, u"registration_number");
    if (!regNum.isEmpty())
        html += "<h2>" + qtTrId("lc-euvrc-doc-reg-number") + ": " + getPreparedValue(regNum) + "</h2>\n";

    html += buildRegistrationSection(groups);
    html += buildVehicleSection(groups);
    html += buildEngineTechnicalSection(groups);
    html += buildHolderSection(groups);
    html += buildOwnerSection(groups);
    html += buildUserSection(groups);
    html += buildNationalSection(groups);

    // Printing date
    html += "<table style=\"margin-top:20px;\"><tr>"
            "<td width=\"0\"><img src=\":/images/transparent_1x20.png\" width=\"1\" height=\"8\"></td>"
            "<td width=\"25%\">" +
            qtTrId("lc-euvrc-doc-printing-date") +
            ":</td>"
            "<td>" +
            QDate::currentDate().toString("dd.MM.yyyy") +
            "</td>"
            "</tr></table>\n";

    html += "</body>\n</html>\n";
    return html;
}

QString EuVrcTextDocument::buildRegistrationSection(const FieldGroupList& groups) const
{
    struct Field
    {
        QStringView key;
        QString label;
    };
    std::vector<Field> fields = {
        {u"date_of_first_registration", qtTrId("lc-euvrc-doc-first-reg-date")},
        {u"registration_date", qtTrId("lc-euvrc-doc-reg-date")},
        {u"expiry_date", qtTrId("lc-euvrc-doc-expiry-date")},
        {u"member_state", qtTrId("lc-euvrc-doc-member-state")},
        {u"document_number", qtTrId("lc-euvrc-doc-document-number")},
        {u"competent_authority", qtTrId("lc-euvrc-doc-competent-authority")},
        {u"issuing_authority", qtTrId("lc-euvrc-doc-issuing-authority")},
        {u"type_approval_number", qtTrId("lc-euvrc-doc-type-approval-no")},
        {u"ownership_status", qtTrId("lc-euvrc-doc-ownership-status")},
        {u"previous_document", qtTrId("lc-euvrc-doc-previous-document")},
    };

    QString rows;
    for (const auto& f : fields) {
        auto val = fieldValue(groups, f.key);
        QString cssClass;
        if (f.key == u"expiry_date" && !val.isEmpty()) {
            auto expiry = QDate::fromString(val, "dd.MM.yyyy");
            if (expiry.isValid() && expiry < QDate::currentDate())
                cssClass = "expired";
        }
        rows += emitRow(f.label, val, cssClass);
    }

    if (rows.isEmpty())
        return {};

    return "<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildVehicleSection(const FieldGroupList& groups) const
{
    struct Field
    {
        QStringView key;
        QString label;
    };
    std::vector<Field> fields = {
        {u"vehicle_make", qtTrId("lc-euvrc-doc-make")},
        {u"vehicle_type", qtTrId("lc-euvrc-doc-type")},
        {u"commercial_description", qtTrId("lc-euvrc-doc-commercial-desc")},
        {u"vehicle_id_number", qtTrId("lc-euvrc-doc-vin")},
        {u"vehicle_category", qtTrId("lc-euvrc-doc-category")},
        {u"colour", qtTrId("lc-euvrc-doc-colour")},
        {u"max_speed", qtTrId("lc-euvrc-doc-max-speed")},
    };

    QString rows;
    for (const auto& f : fields)
        rows += emitRow(f.label, fieldValue(groups, f.key));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-vehicle-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildEngineTechnicalSection(const FieldGroupList& groups) const
{
    struct Field
    {
        QStringView key;
        QString label;
    };
    std::vector<Field> fields = {
        {u"engine_capacity", qtTrId("lc-euvrc-doc-capacity")},
        {u"maximum_net_power", qtTrId("lc-euvrc-doc-power")},
        {u"type_of_fuel", qtTrId("lc-euvrc-doc-fuel-type")},
        {u"engine_id_number", qtTrId("lc-euvrc-doc-engine-number")},
        {u"vehicle_mass", qtTrId("lc-euvrc-doc-mass")},
        {u"maximum_permissible_laden_mass", qtTrId("lc-euvrc-doc-max-laden-mass")},
        {u"max_laden_mass_service", qtTrId("lc-euvrc-doc-max-laden-mass-service")},
        {u"max_laden_mass_whole", qtTrId("lc-euvrc-doc-max-laden-mass-whole")},
        {u"power_weight_ratio", qtTrId("lc-euvrc-doc-power-weight")},
        {u"number_of_seats", qtTrId("lc-euvrc-doc-seats")},
        {u"number_of_standing_places", qtTrId("lc-euvrc-doc-standing-places")},
        {u"number_of_axles", qtTrId("lc-euvrc-doc-axles")},
        {u"wheelbase", qtTrId("lc-euvrc-doc-wheelbase")},
        {u"braked_trailer_mass", qtTrId("lc-euvrc-doc-braked-trailer")},
        {u"unbraked_trailer_mass", qtTrId("lc-euvrc-doc-unbraked-trailer")},
        {u"rated_engine_speed", qtTrId("lc-euvrc-doc-rated-engine-speed")},
        {u"stationary_sound_level", qtTrId("lc-euvrc-doc-stationary-sound")},
        {u"engine_speed_ref", qtTrId("lc-euvrc-doc-engine-speed-ref")},
        {u"drive_by_sound", qtTrId("lc-euvrc-doc-drive-by-sound")},
        {u"fuel_consumption", qtTrId("lc-euvrc-doc-fuel-consumption")},
        {u"co2_emissions", qtTrId("lc-euvrc-doc-co2")},
        {u"environmental_category", qtTrId("lc-euvrc-doc-env-category")},
        {u"fuel_tank_capacity", qtTrId("lc-euvrc-doc-fuel-tank")},
    };

    QString rows;
    for (const auto& f : fields)
        rows += emitRow(f.label, fieldValue(groups, f.key));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-engine-technical") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildHolderSection(const FieldGroupList& groups) const
{
    struct Field
    {
        QStringView key;
        QString label;
        bool isAddress;
    };
    std::vector<Field> fields = {
        {u"holder_name", qtTrId("lc-euvrc-doc-holder-name"), false},
        {u"holder_other_names", qtTrId("lc-euvrc-doc-holder-other-names"), false},
        {u"holder_address", qtTrId("lc-euvrc-doc-holder-address"), true},
    };

    QString rows;
    for (const auto& f : fields) {
        auto val = fieldValue(groups, f.key);
        if (f.isAddress && !val.isEmpty())
            val = cleanAddress(val);
        rows += emitRow(f.label, val);
    }

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-holder-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildOwnerSection(const FieldGroupList& groups) const
{
    auto val = fieldValue(groups, u"owner2_name");
    auto row = emitRow(qtTrId("lc-euvrc-doc-owner-name"), val);
    if (row.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-owner-data") + "</h2>\n<table>\n" + row + "</table>\n";
}

QString EuVrcTextDocument::buildUserSection(const FieldGroupList& groups) const
{
    struct Field
    {
        QStringView key;
        QString label;
        bool isAddress;
    };
    std::vector<Field> fields = {
        {u"user_name", qtTrId("lc-euvrc-doc-user-name"), false},
        {u"user_other_names", qtTrId("lc-euvrc-doc-user-other-names"), false},
        {u"user_address", qtTrId("lc-euvrc-doc-user-address"), true},
    };

    QString rows;
    for (const auto& f : fields) {
        auto val = fieldValue(groups, f.key);
        if (f.isAddress && !val.isEmpty())
            val = cleanAddress(val);
        rows += emitRow(f.label, val);
    }

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-user-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildNationalSection(const FieldGroupList& groups) const
{
    const auto* natGroup = findGroup(groups, u"national");
    if (!natGroup)
        return {};
    if (natGroup->fields.isEmpty())
        return {};

    // Known Serbian national extension labels
    const std::map<QString, QString> knownLabels = {
        {QStringLiteral("owners_personal_no"), qtTrId("lc-euvrc-nat-owners-personal-no")},
        {QStringLiteral("users_personal_no"), qtTrId("lc-euvrc-nat-users-personal-no")},
        {QStringLiteral("vehicle_load"), qtTrId("lc-euvrc-nat-vehicle-load")},
        {QStringLiteral("year_of_production"), qtTrId("lc-euvrc-nat-year-of-production")},
        {QStringLiteral("serial_number"), qtTrId("lc-euvrc-nat-serial-number")},
    };

    QString rows;
    for (const auto& field : natGroup->fields) {
        const QString val = field.value;
        if (val.isEmpty())
            continue;

        auto it = knownLabels.find(field.key);
        QString label =
            (it != knownLabels.end()) ? it->second : field.extra.value(QStringLiteral("labelFallback")).toString();
        if (label.isEmpty())
            label = field.key;

        rows += emitRow(label, val);
    }

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-national-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}
