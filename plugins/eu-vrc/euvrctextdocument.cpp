// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "euvrctextdocument.h"
#include "utils/stringutils.h"
#include <plugin/carddatautils.h>

using LibreSCRS::Plugin::getFieldValue;

EuVrcTextDocument::EuVrcTextDocument(const LibreSCRS::Plugin::CardData& cardData, QString cssPath)
{
    auto html = buildHtml(cardData);
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

QString EuVrcTextDocument::buildHtml(const LibreSCRS::Plugin::CardData& cardData) const
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
    auto regNum = getFieldValue(cardData, "registration_number");
    if (!regNum.isEmpty())
        html += "<h2>" + qtTrId("lc-euvrc-doc-reg-number") + ": " + getPreparedValue(regNum) + "</h2>\n";

    html += buildRegistrationSection(cardData);
    html += buildVehicleSection(cardData);
    html += buildEngineTechnicalSection(cardData);
    html += buildHolderSection(cardData);
    html += buildOwnerSection(cardData);
    html += buildUserSection(cardData);
    html += buildNationalSection(cardData);

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

QString EuVrcTextDocument::buildRegistrationSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    struct Field
    {
        const char* key;
        QString label;
    };
    std::vector<Field> fields = {
        {"date_of_first_registration", qtTrId("lc-euvrc-doc-first-reg-date")},
        {"registration_date", qtTrId("lc-euvrc-doc-reg-date")},
        {"expiry_date", qtTrId("lc-euvrc-doc-expiry-date")},
        {"member_state", qtTrId("lc-euvrc-doc-member-state")},
        {"document_number", qtTrId("lc-euvrc-doc-document-number")},
        {"competent_authority", qtTrId("lc-euvrc-doc-competent-authority")},
        {"issuing_authority", qtTrId("lc-euvrc-doc-issuing-authority")},
        {"type_approval_number", qtTrId("lc-euvrc-doc-type-approval-no")},
        {"ownership_status", qtTrId("lc-euvrc-doc-ownership-status")},
        {"previous_document", qtTrId("lc-euvrc-doc-previous-document")},
    };

    QString rows;
    for (const auto& f : fields) {
        auto val = getFieldValue(cardData, f.key);
        QString cssClass;
        if (std::string_view(f.key) == "expiry_date" && !val.isEmpty()) {
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

QString EuVrcTextDocument::buildVehicleSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    struct Field
    {
        const char* key;
        QString label;
    };
    std::vector<Field> fields = {
        {"vehicle_make", qtTrId("lc-euvrc-doc-make")},
        {"vehicle_type", qtTrId("lc-euvrc-doc-type")},
        {"commercial_description", qtTrId("lc-euvrc-doc-commercial-desc")},
        {"vehicle_id_number", qtTrId("lc-euvrc-doc-vin")},
        {"vehicle_category", qtTrId("lc-euvrc-doc-category")},
        {"colour", qtTrId("lc-euvrc-doc-colour")},
        {"max_speed", qtTrId("lc-euvrc-doc-max-speed")},
    };

    QString rows;
    for (const auto& f : fields)
        rows += emitRow(f.label, getFieldValue(cardData, f.key));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-vehicle-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildEngineTechnicalSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    struct Field
    {
        const char* key;
        QString label;
    };
    std::vector<Field> fields = {
        {"engine_capacity", qtTrId("lc-euvrc-doc-capacity")},
        {"maximum_net_power", qtTrId("lc-euvrc-doc-power")},
        {"type_of_fuel", qtTrId("lc-euvrc-doc-fuel-type")},
        {"engine_id_number", qtTrId("lc-euvrc-doc-engine-number")},
        {"vehicle_mass", qtTrId("lc-euvrc-doc-mass")},
        {"maximum_permissible_laden_mass", qtTrId("lc-euvrc-doc-max-laden-mass")},
        {"max_laden_mass_service", qtTrId("lc-euvrc-doc-max-laden-mass-service")},
        {"max_laden_mass_whole", qtTrId("lc-euvrc-doc-max-laden-mass-whole")},
        {"power_weight_ratio", qtTrId("lc-euvrc-doc-power-weight")},
        {"number_of_seats", qtTrId("lc-euvrc-doc-seats")},
        {"number_of_standing_places", qtTrId("lc-euvrc-doc-standing-places")},
        {"number_of_axles", qtTrId("lc-euvrc-doc-axles")},
        {"wheelbase", qtTrId("lc-euvrc-doc-wheelbase")},
        {"braked_trailer_mass", qtTrId("lc-euvrc-doc-braked-trailer")},
        {"unbraked_trailer_mass", qtTrId("lc-euvrc-doc-unbraked-trailer")},
        {"rated_engine_speed", qtTrId("lc-euvrc-doc-rated-engine-speed")},
        {"stationary_sound_level", qtTrId("lc-euvrc-doc-stationary-sound")},
        {"engine_speed_ref", qtTrId("lc-euvrc-doc-engine-speed-ref")},
        {"drive_by_sound", qtTrId("lc-euvrc-doc-drive-by-sound")},
        {"fuel_consumption", qtTrId("lc-euvrc-doc-fuel-consumption")},
        {"co2_emissions", qtTrId("lc-euvrc-doc-co2")},
        {"environmental_category", qtTrId("lc-euvrc-doc-env-category")},
        {"fuel_tank_capacity", qtTrId("lc-euvrc-doc-fuel-tank")},
    };

    QString rows;
    for (const auto& f : fields)
        rows += emitRow(f.label, getFieldValue(cardData, f.key));

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-engine-technical") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildHolderSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    struct Field
    {
        const char* key;
        QString label;
        bool isAddress;
    };
    std::vector<Field> fields = {
        {"holder_name", qtTrId("lc-euvrc-doc-holder-name"), false},
        {"holder_other_names", qtTrId("lc-euvrc-doc-holder-other-names"), false},
        {"holder_address", qtTrId("lc-euvrc-doc-holder-address"), true},
    };

    QString rows;
    for (const auto& f : fields) {
        auto val = getFieldValue(cardData, f.key);
        if (f.isAddress && !val.isEmpty())
            val = cleanAddress(val);
        rows += emitRow(f.label, val);
    }

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-holder-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildOwnerSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    auto val = getFieldValue(cardData, "owner2_name");
    auto row = emitRow(qtTrId("lc-euvrc-doc-owner-name"), val);
    if (row.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-owner-data") + "</h2>\n<table>\n" + row + "</table>\n";
}

QString EuVrcTextDocument::buildUserSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    struct Field
    {
        const char* key;
        QString label;
        bool isAddress;
    };
    std::vector<Field> fields = {
        {"user_name", qtTrId("lc-euvrc-doc-user-name"), false},
        {"user_other_names", qtTrId("lc-euvrc-doc-user-other-names"), false},
        {"user_address", qtTrId("lc-euvrc-doc-user-address"), true},
    };

    QString rows;
    for (const auto& f : fields) {
        auto val = getFieldValue(cardData, f.key);
        if (f.isAddress && !val.isEmpty())
            val = cleanAddress(val);
        rows += emitRow(f.label, val);
    }

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-user-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}

QString EuVrcTextDocument::buildNationalSection(const LibreSCRS::Plugin::CardData& cardData) const
{
    auto natGroupOpt = cardData.findGroup("national");
    if (!natGroupOpt)
        return {};
    const auto& natGroup = cardData.groupAt(*natGroupOpt);
    if (natGroup.fields.empty())
        return {};

    // Known Serbian national extension labels
    const std::map<std::string, QString> knownLabels = {
        {"owners_personal_no", qtTrId("lc-euvrc-nat-owners-personal-no")},
        {"users_personal_no", qtTrId("lc-euvrc-nat-users-personal-no")},
        {"vehicle_load", qtTrId("lc-euvrc-nat-vehicle-load")},
        {"year_of_production", qtTrId("lc-euvrc-nat-year-of-production")},
        {"serial_number", qtTrId("lc-euvrc-nat-serial-number")},
    };

    QString rows;
    for (const auto& field : natGroup.fields) {
        auto textOpt = field.textValue();
        if (!textOpt.has_value())
            continue;
        auto val = QString::fromStdString(*textOpt);
        if (val.isEmpty())
            continue;

        auto it = knownLabels.find(field.key);
        QString label = (it != knownLabels.end()) ? it->second : QString::fromStdString(field.label);
        if (label.isEmpty())
            label = QString::fromStdString(field.key);

        rows += emitRow(label, val);
    }

    if (rows.isEmpty())
        return {};

    return "<h2>" + qtTrId("lc-euvrc-doc-national-data") + "</h2>\n<table>\n" + rows + "</table>\n";
}
