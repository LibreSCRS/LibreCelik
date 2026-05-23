// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "euvrcwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"
#include "utils/stringutils.h"

#include <plugin/carddatautils.h>

#include <QDate>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>

using librecelik::plugin::getFieldValue;

EuVrcWidget::EuVrcWidget(const LibreSCRS::Plugin::CardData& cardData, QWidget* parent) : EuVrcWidget(parent)
{
    data.cardType = cardData.cardType;
    for (const auto& group : cardData.groups)
        addGroup(group);
}

EuVrcWidget::EuVrcWidget(QWidget* parent) : plugin_ui::PluginWidgetBase(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    buildShell();
}

void EuVrcWidget::buildShell()
{
    outerSection = new CollapsibleSection(qtTrId("lc-euvrc-title"), QColor(34, 86, 117), this);
    outerSection->setHeaderHeight(56);

    contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(8);

    outerSection->setLayout(contentLayout);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    outerLayout->addWidget(outerSection);

    // Print button — disabled until all data arrives
    printBtn = iconutils::createPrinterHeaderButton(this);
    connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(data); });
    outerSection->addHeaderWidget(printBtn);
}

void EuVrcWidget::addGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // Accumulate into data for cardData() accessor
    data.groups.push_back(group);

    if (group.groupKey == "registration")
        addRegistrationGroup(group);
    else if (group.groupKey == "vehicle")
        addVehicleGroup(group);
    else if (group.groupKey == "holder")
        addHolderGroup(group);
    else if (group.groupKey == "owner")
        addOwnerGroup(group);
    else if (group.groupKey == "user")
        addUserGroup(group);
    else if (group.groupKey == "national")
        addNationalGroup(group);
}

void EuVrcWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

void EuVrcWidget::addRegistrationGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // CardHeaderCard: icon + key fields (make is added later when vehicle group arrives)
    auto regNumber = getFieldValue(&group, "registration_number");
    auto expiry = getFieldValue(&group, "expiry_date");
    auto memberState = getFieldValue(&group, "member_state");

    std::vector<librecelik::utils::HeaderField> headerFields = {
        {qtTrId("lc-euvrc-hdr-registration"), regNumber, 2},
        {qtTrId("lc-euvrc-hdr-member-state"), memberState},
        {qtTrId("lc-euvrc-hdr-valid-to"), expiry},
    };

    headerCard = new librecelik::utils::CardHeaderCard(QIcon(":/images/vehicle-icon.svg"), QSize(80, 80), headerFields,
                                                       outerSection);
    contentLayout->addWidget(headerCard);

    contentLayout->addWidget(buildRegistrationSection(&group));
}

void EuVrcWidget::addVehicleGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // Update header card with vehicle make now that the vehicle group has arrived
    if (headerCard) {
        if (auto regIdx = data.findGroup("registration")) {
            const auto& regGroup = data.groupAt(*regIdx);
            auto regNumber = getFieldValue(&regGroup, "registration_number");
            auto expiry = getFieldValue(&regGroup, "expiry_date");
            auto memberState = getFieldValue(&regGroup, "member_state");
            auto make = getFieldValue(&group, "vehicle_make");

            std::vector<librecelik::utils::HeaderField> headerFields = {
                {qtTrId("lc-euvrc-hdr-registration"), regNumber, 2},
                {qtTrId("lc-euvrc-hdr-make"), make},
                {qtTrId("lc-euvrc-hdr-member-state"), memberState},
                {qtTrId("lc-euvrc-hdr-valid-to"), expiry},
            };

            auto* newHeader = new librecelik::utils::CardHeaderCard(QIcon(":/images/vehicle-icon.svg"), QSize(80, 80),
                                                                    headerFields, outerSection);
            delete contentLayout->replaceWidget(headerCard, newHeader);
            headerCard->deleteLater();
            headerCard = newHeader;
        }
    }

    contentLayout->addWidget(buildVehicleSection(&group));
    contentLayout->addWidget(buildEngineTechnicalSection(&group));
}

void EuVrcWidget::addHolderGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    contentLayout->addWidget(buildHolderSection(&group));
}

void EuVrcWidget::addOwnerGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    bool hasValues = false;
    for (const auto& field : group.fields) {
        auto text = field.textValue();
        if (text.has_value() && !text->empty()) {
            hasValues = true;
            break;
        }
    }
    if (hasValues)
        contentLayout->addWidget(buildOwnerSection(&group));
}

void EuVrcWidget::addUserGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // Only add if there are actual fields with values
    bool hasValues = false;
    for (const auto& field : group.fields) {
        auto text = field.textValue();
        if (text.has_value() && !text->empty()) {
            hasValues = true;
            break;
        }
    }
    if (hasValues)
        contentLayout->addWidget(buildUserSection(&group));
}

void EuVrcWidget::addNationalGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    if (!group.fields.empty())
        contentLayout->addWidget(buildNationalSection(&group));
}

CollapsibleSection* EuVrcWidget::buildRegistrationSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    LibreSCRS::Plugin::CardFieldGroup regGroup;
    regGroup.groupKey = "registration_display";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            regGroup.fields.push_back(
                {key, key, LibreSCRS::Plugin::FieldType::Text, {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    };

    addIfPresent("registration_number");
    addIfPresent("date_of_first_registration");
    addIfPresent("registration_date");
    addIfPresent("expiry_date");
    addIfPresent("document_number");
    addIfPresent("issuing_authority");
    addIfPresent("competent_authority");
    addIfPresent("member_state");
    addIfPresent("type_approval_number");
    addIfPresent("ownership_status");
    addIfPresent("previous_document");

    std::map<std::string, QString> labels = {
        {"registration_number", qtTrId("lc-euvrc-reg-number")},
        {"date_of_first_registration", qtTrId("lc-euvrc-reg-first-date")},
        {"registration_date", qtTrId("lc-euvrc-reg-date")},
        {"expiry_date", qtTrId("lc-euvrc-reg-expiry")},
        {"document_number", qtTrId("lc-euvrc-reg-doc-number")},
        {"issuing_authority", qtTrId("lc-euvrc-reg-issuing-auth")},
        {"competent_authority", qtTrId("lc-euvrc-reg-competent-auth")},
        {"member_state", qtTrId("lc-euvrc-reg-member-state")},
        {"type_approval_number", qtTrId("lc-euvrc-reg-type-approval")},
        {"ownership_status", qtTrId("lc-euvrc-reg-ownership-status")},
        {"previous_document", qtTrId("lc-euvrc-reg-previous-document")},
    };

    auto* section =
        librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-registration"), regGroup, labels);

    // Apply orange highlight to expiry date if expired
    librecelik::utils::FieldSectionBuilder::highlightExpiredDates(section, regGroup, {"expiry_date"});

    return section;
}

CollapsibleSection* EuVrcWidget::buildVehicleSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    LibreSCRS::Plugin::CardFieldGroup vehGroup;
    vehGroup.groupKey = "vehicle_display";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            vehGroup.fields.push_back(
                {key, key, LibreSCRS::Plugin::FieldType::Text, {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    };

    addIfPresent("vehicle_make");
    addIfPresent("vehicle_type");
    addIfPresent("commercial_description");
    addIfPresent("vehicle_id_number");
    addIfPresent("vehicle_category");
    addIfPresent("colour");
    addIfPresent("max_speed");

    std::map<std::string, QString> labels = {
        {"vehicle_make", qtTrId("lc-euvrc-veh-make")},
        {"vehicle_type", qtTrId("lc-euvrc-veh-type")},
        {"commercial_description", qtTrId("lc-euvrc-veh-commercial-desc")},
        {"vehicle_id_number", qtTrId("lc-euvrc-veh-vin")},
        {"vehicle_category", qtTrId("lc-euvrc-veh-category")},
        {"colour", qtTrId("lc-euvrc-veh-colour")},
        {"max_speed", qtTrId("lc-euvrc-veh-max-speed")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-vehicle"), vehGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildEngineTechnicalSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    LibreSCRS::Plugin::CardFieldGroup techGroup;
    techGroup.groupKey = "engine_technical";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            techGroup.fields.push_back(
                {key, key, LibreSCRS::Plugin::FieldType::Text, {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    };

    addIfPresent("engine_capacity");
    addIfPresent("maximum_net_power");
    addIfPresent("type_of_fuel");
    addIfPresent("engine_id_number");
    addIfPresent("vehicle_mass");
    addIfPresent("maximum_permissible_laden_mass");
    addIfPresent("power_weight_ratio");
    addIfPresent("number_of_seats");
    addIfPresent("number_of_standing_places");
    addIfPresent("number_of_axles");
    addIfPresent("wheelbase");
    addIfPresent("max_laden_mass_service");
    addIfPresent("max_laden_mass_whole");
    addIfPresent("braked_trailer_mass");
    addIfPresent("unbraked_trailer_mass");
    addIfPresent("rated_engine_speed");
    addIfPresent("stationary_sound_level");
    addIfPresent("engine_speed_ref");
    addIfPresent("drive_by_sound");
    addIfPresent("fuel_consumption");
    addIfPresent("co2_emissions");
    addIfPresent("environmental_category");
    addIfPresent("fuel_tank_capacity");

    std::map<std::string, QString> labels = {
        {"engine_capacity", qtTrId("lc-euvrc-eng-capacity")},
        {"maximum_net_power", qtTrId("lc-euvrc-eng-max-power")},
        {"type_of_fuel", qtTrId("lc-euvrc-eng-fuel-type")},
        {"engine_id_number", qtTrId("lc-euvrc-eng-id-number")},
        {"vehicle_mass", qtTrId("lc-euvrc-eng-vehicle-mass")},
        {"maximum_permissible_laden_mass", qtTrId("lc-euvrc-eng-max-laden-mass")},
        {"power_weight_ratio", qtTrId("lc-euvrc-eng-power-weight")},
        {"number_of_seats", qtTrId("lc-euvrc-eng-seats")},
        {"number_of_standing_places", qtTrId("lc-euvrc-eng-standing")},
        {"number_of_axles", qtTrId("lc-euvrc-eng-axles")},
        {"wheelbase", qtTrId("lc-euvrc-eng-wheelbase")},
        {"max_laden_mass_service", qtTrId("lc-euvrc-eng-max-laden-mass-service")},
        {"max_laden_mass_whole", qtTrId("lc-euvrc-eng-max-laden-mass-whole")},
        {"braked_trailer_mass", qtTrId("lc-euvrc-eng-braked-trailer")},
        {"unbraked_trailer_mass", qtTrId("lc-euvrc-eng-unbraked-trailer")},
        {"rated_engine_speed", qtTrId("lc-euvrc-eng-rated-engine-speed")},
        {"stationary_sound_level", qtTrId("lc-euvrc-eng-stationary-sound")},
        {"engine_speed_ref", qtTrId("lc-euvrc-eng-engine-speed-ref")},
        {"drive_by_sound", qtTrId("lc-euvrc-eng-drive-by-sound")},
        {"fuel_consumption", qtTrId("lc-euvrc-eng-fuel-consumption")},
        {"co2_emissions", qtTrId("lc-euvrc-eng-co2")},
        {"environmental_category", qtTrId("lc-euvrc-eng-env-category")},
        {"fuel_tank_capacity", qtTrId("lc-euvrc-eng-fuel-tank")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-engine"), techGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildHolderSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    LibreSCRS::Plugin::CardFieldGroup holderGroup;
    holderGroup.groupKey = "holder_display";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            holderGroup.fields.push_back(
                {key, key, LibreSCRS::Plugin::FieldType::Text, {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    };

    addIfPresent("holder_name");
    addIfPresent("holder_other_names");
    {
        auto val = getFieldValue(group, "holder_address");
        if (!val.isEmpty()) {
            val = cleanAddress(val);
            auto bytes = val.toUtf8();
            holderGroup.fields.push_back({"holder_address",
                                          "holder_address",
                                          LibreSCRS::Plugin::FieldType::Text,
                                          {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    }

    std::map<std::string, QString> labels = {
        {"holder_name", qtTrId("lc-euvrc-holder-name")},
        {"holder_other_names", qtTrId("lc-euvrc-holder-other-names")},
        {"holder_address", qtTrId("lc-euvrc-holder-address")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-holder"), holderGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildOwnerSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    LibreSCRS::Plugin::CardFieldGroup ownerGroup;
    ownerGroup.groupKey = "owner_display";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            ownerGroup.fields.push_back(
                {key, key, LibreSCRS::Plugin::FieldType::Text, {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    };

    addIfPresent("owner2_name");

    std::map<std::string, QString> labels = {
        {"owner2_name", qtTrId("lc-euvrc-owner-name")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-owner"), ownerGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildUserSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    LibreSCRS::Plugin::CardFieldGroup userGroup;
    userGroup.groupKey = "user_display";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            userGroup.fields.push_back(
                {key, key, LibreSCRS::Plugin::FieldType::Text, {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    };

    addIfPresent("user_name");
    addIfPresent("user_other_names");
    {
        auto val = getFieldValue(group, "user_address");
        if (!val.isEmpty()) {
            val = cleanAddress(val);
            auto bytes = val.toUtf8();
            userGroup.fields.push_back({"user_address",
                                        "user_address",
                                        LibreSCRS::Plugin::FieldType::Text,
                                        {bytes.constData(), bytes.constData() + bytes.size()}});
        }
    }

    std::map<std::string, QString> labels = {
        {"user_name", qtTrId("lc-euvrc-user-name")},
        {"user_other_names", qtTrId("lc-euvrc-user-other-names")},
        {"user_address", qtTrId("lc-euvrc-user-address")},
    };

    auto* section = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-user"), userGroup, labels);

    // Hide when all fields are empty
    if (userGroup.fields.empty())
        section->setVisible(false);

    return section;
}

CollapsibleSection* EuVrcWidget::buildNationalSection(const LibreSCRS::Plugin::CardFieldGroup* group)
{
    // Translated labels for known Serbian national extension keys
    const std::map<std::string, QString> knownLabels = {
        {"owners_personal_no", qtTrId("lc-euvrc-nat-owners-personal-no")},
        {"users_personal_no", qtTrId("lc-euvrc-nat-users-personal-no")},
        {"vehicle_load", qtTrId("lc-euvrc-nat-vehicle-load")},
        {"year_of_production", qtTrId("lc-euvrc-nat-year-of-production")},
        {"serial_number", qtTrId("lc-euvrc-nat-serial-number")},
    };

    LibreSCRS::Plugin::CardFieldGroup natGroup;
    natGroup.groupKey = "national_display";

    std::map<std::string, QString> labels;

    for (const auto& field : group->fields) {
        auto textOpt = field.textValue();
        if (!textOpt.has_value())
            continue;
        auto val = QString::fromStdString(*textOpt);
        if (!val.isEmpty()) {
            auto bytes = val.toUtf8();
            natGroup.fields.push_back({field.key,
                                       field.key,
                                       LibreSCRS::Plugin::FieldType::Text,
                                       {bytes.constData(), bytes.constData() + bytes.size()}});

            auto it = knownLabels.find(field.key);
            if (it != knownLabels.end()) {
                labels[field.key] = it->second;
            } else {
                QString displayLabel = QString::fromStdString(field.label);
                if (displayLabel.isEmpty())
                    displayLabel = QString::fromStdString(field.key);
                labels[field.key] = displayLabel;
            }
        }
    }

    auto* section =
        librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-national"), natGroup, labels);

    return section;
}

void EuVrcWidget::retranslateUi()
{
    // Plugin widget rebuild-tier (April 2026 retranslate spec): tear
    // down the shell and rebuild from cached data.groups.
    auto cachedGroups = std::move(data.groups);
    data.groups.clear();

    if (outerSection) {
        outerLayout->removeWidget(outerSection);
        outerSection->deleteLater();
        outerSection = nullptr;
    }
    contentLayout = nullptr;
    headerCard = nullptr;
    printBtn = nullptr;

    buildShell();
    for (const auto& group : cachedGroups)
        addGroup(group);
}
