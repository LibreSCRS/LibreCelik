// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "euvrcwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"
#include "utils/stringutils.h"

#include <plugin/fieldvalue.h>

#include <QDate>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QToolButton>
#include <QVBoxLayout>

using librecelik::plugin::fieldValue;
using librecelik::plugin::findGroup;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

/// The outgoing helper's nullable-group overload, ported: the section builders
/// take their group by pointer, and a missing group renders as empty rather
/// than dereferencing.
QString groupValue(const FieldGroup* group, QStringView key)
{
    return group ? fieldValue(*group, key) : QString{};
}

/// A display-only field for the section builder. The value is already
/// stringified, the label fallback is the key itself (the outgoing model put
/// the key in the label slot too), and the type token keeps the shared flatten
/// rule from mistaking a value-carrying row for a binary payload.
Field displayField(const QString& key, const QString& value)
{
    Field field;
    field.key = key;
    field.value = value;
    field.extra.insert(QStringLiteral("labelFallback"), key);
    field.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
    return field;
}

} // namespace

EuVrcWidget::EuVrcWidget(const QList<FieldGroup>& cardGroups, QWidget* parent) : EuVrcWidget(parent)
{
    // Staged: the registration group raises the car-icon header card every
    // later section hangs under, and the final wire model's order is
    // delivery-dependent (the Leg-6 bench catch: the header rendered
    // mid-page from a recovered read's keyed order).
    for (const auto& group : librecelik::plugin::stagedForBuild(
             cardGroups, {u"registration", u"vehicle", u"holder", u"owner", u"user", u"national"}))
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
    connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(groups); });
    outerSection->addHeaderWidget(printBtn);
}

void EuVrcWidget::addGroup(const FieldGroup& group)
{
    // Accumulate into groups for the fieldGroups() accessor
    groups.append(group);

    if (group.key == QLatin1String("registration"))
        addRegistrationGroup(group);
    else if (group.key == QLatin1String("vehicle"))
        addVehicleGroup(group);
    else if (group.key == QLatin1String("holder"))
        addHolderGroup(group);
    else if (group.key == QLatin1String("owner"))
        addOwnerGroup(group);
    else if (group.key == QLatin1String("user"))
        addUserGroup(group);
    else if (group.key == QLatin1String("national"))
        addNationalGroup(group);
}

void EuVrcWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

void EuVrcWidget::addRegistrationGroup(const FieldGroup& group)
{
    // CardHeaderCard: icon + key fields (make is added later when vehicle group arrives)
    auto regNumber = fieldValue(group, u"registration_number");
    auto expiry = fieldValue(group, u"expiry_date");
    auto memberState = fieldValue(group, u"member_state");

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

void EuVrcWidget::addVehicleGroup(const FieldGroup& group)
{
    // Update header card with vehicle make now that the vehicle group has arrived
    if (headerCard) {
        if (const FieldGroup* regGroup = findGroup(groups, u"registration")) {
            auto regNumber = fieldValue(*regGroup, u"registration_number");
            auto expiry = fieldValue(*regGroup, u"expiry_date");
            auto memberState = fieldValue(*regGroup, u"member_state");
            auto make = fieldValue(group, u"vehicle_make");

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

void EuVrcWidget::addHolderGroup(const FieldGroup& group)
{
    contentLayout->addWidget(buildHolderSection(&group));
}

void EuVrcWidget::addOwnerGroup(const FieldGroup& group)
{
    bool hasValues = false;
    for (const auto& field : group.fields) {
        if (!field.value.isEmpty()) {
            hasValues = true;
            break;
        }
    }
    if (hasValues)
        contentLayout->addWidget(buildOwnerSection(&group));
}

void EuVrcWidget::addUserGroup(const FieldGroup& group)
{
    // Only add if there are actual fields with values
    bool hasValues = false;
    for (const auto& field : group.fields) {
        if (!field.value.isEmpty()) {
            hasValues = true;
            break;
        }
    }
    if (hasValues)
        contentLayout->addWidget(buildUserSection(&group));
}

void EuVrcWidget::addNationalGroup(const FieldGroup& group)
{
    if (!group.fields.isEmpty())
        contentLayout->addWidget(buildNationalSection(&group));
}

CollapsibleSection* EuVrcWidget::buildRegistrationSection(const FieldGroup* group)
{
    FieldGroup regGroup;
    regGroup.key = QStringLiteral("registration_display");

    auto addIfPresent = [&](const QString& key) {
        auto val = groupValue(group, key);
        if (!val.isEmpty()) {
            regGroup.fields.append(displayField(key, val));
        }
    };

    addIfPresent(QStringLiteral("registration_number"));
    addIfPresent(QStringLiteral("date_of_first_registration"));
    addIfPresent(QStringLiteral("registration_date"));
    addIfPresent(QStringLiteral("expiry_date"));
    addIfPresent(QStringLiteral("document_number"));
    addIfPresent(QStringLiteral("issuing_authority"));
    addIfPresent(QStringLiteral("competent_authority"));
    addIfPresent(QStringLiteral("member_state"));
    addIfPresent(QStringLiteral("type_approval_number"));
    addIfPresent(QStringLiteral("ownership_status"));
    addIfPresent(QStringLiteral("previous_document"));

    std::map<QString, QString> labels = {
        {QStringLiteral("registration_number"), qtTrId("lc-euvrc-reg-number")},
        {QStringLiteral("date_of_first_registration"), qtTrId("lc-euvrc-reg-first-date")},
        {QStringLiteral("registration_date"), qtTrId("lc-euvrc-reg-date")},
        {QStringLiteral("expiry_date"), qtTrId("lc-euvrc-reg-expiry")},
        {QStringLiteral("document_number"), qtTrId("lc-euvrc-reg-doc-number")},
        {QStringLiteral("issuing_authority"), qtTrId("lc-euvrc-reg-issuing-auth")},
        {QStringLiteral("competent_authority"), qtTrId("lc-euvrc-reg-competent-auth")},
        {QStringLiteral("member_state"), qtTrId("lc-euvrc-reg-member-state")},
        {QStringLiteral("type_approval_number"), qtTrId("lc-euvrc-reg-type-approval")},
        {QStringLiteral("ownership_status"), qtTrId("lc-euvrc-reg-ownership-status")},
        {QStringLiteral("previous_document"), qtTrId("lc-euvrc-reg-previous-document")},
    };

    auto* section =
        librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-registration"), regGroup, labels);

    // Apply orange highlight to expiry date if expired
    librecelik::utils::FieldSectionBuilder::highlightExpiredDates(section, regGroup, {QStringLiteral("expiry_date")});

    return section;
}

CollapsibleSection* EuVrcWidget::buildVehicleSection(const FieldGroup* group)
{
    FieldGroup vehGroup;
    vehGroup.key = QStringLiteral("vehicle_display");

    auto addIfPresent = [&](const QString& key) {
        auto val = groupValue(group, key);
        if (!val.isEmpty()) {
            vehGroup.fields.append(displayField(key, val));
        }
    };

    addIfPresent(QStringLiteral("vehicle_make"));
    addIfPresent(QStringLiteral("vehicle_type"));
    addIfPresent(QStringLiteral("commercial_description"));
    addIfPresent(QStringLiteral("vehicle_id_number"));
    addIfPresent(QStringLiteral("vehicle_category"));
    addIfPresent(QStringLiteral("colour"));
    addIfPresent(QStringLiteral("max_speed"));

    std::map<QString, QString> labels = {
        {QStringLiteral("vehicle_make"), qtTrId("lc-euvrc-veh-make")},
        {QStringLiteral("vehicle_type"), qtTrId("lc-euvrc-veh-type")},
        {QStringLiteral("commercial_description"), qtTrId("lc-euvrc-veh-commercial-desc")},
        {QStringLiteral("vehicle_id_number"), qtTrId("lc-euvrc-veh-vin")},
        {QStringLiteral("vehicle_category"), qtTrId("lc-euvrc-veh-category")},
        {QStringLiteral("colour"), qtTrId("lc-euvrc-veh-colour")},
        {QStringLiteral("max_speed"), qtTrId("lc-euvrc-veh-max-speed")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-vehicle"), vehGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildEngineTechnicalSection(const FieldGroup* group)
{
    FieldGroup techGroup;
    techGroup.key = QStringLiteral("engine_technical");

    auto addIfPresent = [&](const QString& key) {
        auto val = groupValue(group, key);
        if (!val.isEmpty()) {
            techGroup.fields.append(displayField(key, val));
        }
    };

    addIfPresent(QStringLiteral("engine_capacity"));
    addIfPresent(QStringLiteral("maximum_net_power"));
    addIfPresent(QStringLiteral("type_of_fuel"));
    addIfPresent(QStringLiteral("engine_id_number"));
    addIfPresent(QStringLiteral("vehicle_mass"));
    addIfPresent(QStringLiteral("maximum_permissible_laden_mass"));
    addIfPresent(QStringLiteral("power_weight_ratio"));
    addIfPresent(QStringLiteral("number_of_seats"));
    addIfPresent(QStringLiteral("number_of_standing_places"));
    addIfPresent(QStringLiteral("number_of_axles"));
    addIfPresent(QStringLiteral("wheelbase"));
    addIfPresent(QStringLiteral("max_laden_mass_service"));
    addIfPresent(QStringLiteral("max_laden_mass_whole"));
    addIfPresent(QStringLiteral("braked_trailer_mass"));
    addIfPresent(QStringLiteral("unbraked_trailer_mass"));
    addIfPresent(QStringLiteral("rated_engine_speed"));
    addIfPresent(QStringLiteral("stationary_sound_level"));
    addIfPresent(QStringLiteral("engine_speed_ref"));
    addIfPresent(QStringLiteral("drive_by_sound"));
    addIfPresent(QStringLiteral("fuel_consumption"));
    addIfPresent(QStringLiteral("co2_emissions"));
    addIfPresent(QStringLiteral("environmental_category"));
    addIfPresent(QStringLiteral("fuel_tank_capacity"));

    std::map<QString, QString> labels = {
        {QStringLiteral("engine_capacity"), qtTrId("lc-euvrc-eng-capacity")},
        {QStringLiteral("maximum_net_power"), qtTrId("lc-euvrc-eng-max-power")},
        {QStringLiteral("type_of_fuel"), qtTrId("lc-euvrc-eng-fuel-type")},
        {QStringLiteral("engine_id_number"), qtTrId("lc-euvrc-eng-id-number")},
        {QStringLiteral("vehicle_mass"), qtTrId("lc-euvrc-eng-vehicle-mass")},
        {QStringLiteral("maximum_permissible_laden_mass"), qtTrId("lc-euvrc-eng-max-laden-mass")},
        {QStringLiteral("power_weight_ratio"), qtTrId("lc-euvrc-eng-power-weight")},
        {QStringLiteral("number_of_seats"), qtTrId("lc-euvrc-eng-seats")},
        {QStringLiteral("number_of_standing_places"), qtTrId("lc-euvrc-eng-standing")},
        {QStringLiteral("number_of_axles"), qtTrId("lc-euvrc-eng-axles")},
        {QStringLiteral("wheelbase"), qtTrId("lc-euvrc-eng-wheelbase")},
        {QStringLiteral("max_laden_mass_service"), qtTrId("lc-euvrc-eng-max-laden-mass-service")},
        {QStringLiteral("max_laden_mass_whole"), qtTrId("lc-euvrc-eng-max-laden-mass-whole")},
        {QStringLiteral("braked_trailer_mass"), qtTrId("lc-euvrc-eng-braked-trailer")},
        {QStringLiteral("unbraked_trailer_mass"), qtTrId("lc-euvrc-eng-unbraked-trailer")},
        {QStringLiteral("rated_engine_speed"), qtTrId("lc-euvrc-eng-rated-engine-speed")},
        {QStringLiteral("stationary_sound_level"), qtTrId("lc-euvrc-eng-stationary-sound")},
        {QStringLiteral("engine_speed_ref"), qtTrId("lc-euvrc-eng-engine-speed-ref")},
        {QStringLiteral("drive_by_sound"), qtTrId("lc-euvrc-eng-drive-by-sound")},
        {QStringLiteral("fuel_consumption"), qtTrId("lc-euvrc-eng-fuel-consumption")},
        {QStringLiteral("co2_emissions"), qtTrId("lc-euvrc-eng-co2")},
        {QStringLiteral("environmental_category"), qtTrId("lc-euvrc-eng-env-category")},
        {QStringLiteral("fuel_tank_capacity"), qtTrId("lc-euvrc-eng-fuel-tank")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-engine"), techGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildHolderSection(const FieldGroup* group)
{
    FieldGroup holderGroup;
    holderGroup.key = QStringLiteral("holder_display");

    auto addIfPresent = [&](const QString& key) {
        auto val = groupValue(group, key);
        if (!val.isEmpty()) {
            holderGroup.fields.append(displayField(key, val));
        }
    };

    addIfPresent(QStringLiteral("holder_name"));
    addIfPresent(QStringLiteral("holder_other_names"));
    {
        auto val = groupValue(group, u"holder_address");
        if (!val.isEmpty()) {
            val = cleanAddress(val);
            holderGroup.fields.append(displayField(QStringLiteral("holder_address"), val));
        }
    }

    std::map<QString, QString> labels = {
        {QStringLiteral("holder_name"), qtTrId("lc-euvrc-holder-name")},
        {QStringLiteral("holder_other_names"), qtTrId("lc-euvrc-holder-other-names")},
        {QStringLiteral("holder_address"), qtTrId("lc-euvrc-holder-address")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-holder"), holderGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildOwnerSection(const FieldGroup* group)
{
    FieldGroup ownerGroup;
    ownerGroup.key = QStringLiteral("owner_display");

    auto addIfPresent = [&](const QString& key) {
        auto val = groupValue(group, key);
        if (!val.isEmpty()) {
            ownerGroup.fields.append(displayField(key, val));
        }
    };

    addIfPresent(QStringLiteral("owner2_name"));

    std::map<QString, QString> labels = {
        {QStringLiteral("owner2_name"), qtTrId("lc-euvrc-owner-name")},
    };

    return librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-owner"), ownerGroup, labels);
}

CollapsibleSection* EuVrcWidget::buildUserSection(const FieldGroup* group)
{
    FieldGroup userGroup;
    userGroup.key = QStringLiteral("user_display");

    auto addIfPresent = [&](const QString& key) {
        auto val = groupValue(group, key);
        if (!val.isEmpty()) {
            userGroup.fields.append(displayField(key, val));
        }
    };

    addIfPresent(QStringLiteral("user_name"));
    addIfPresent(QStringLiteral("user_other_names"));
    {
        auto val = groupValue(group, u"user_address");
        if (!val.isEmpty()) {
            val = cleanAddress(val);
            userGroup.fields.append(displayField(QStringLiteral("user_address"), val));
        }
    }

    std::map<QString, QString> labels = {
        {QStringLiteral("user_name"), qtTrId("lc-euvrc-user-name")},
        {QStringLiteral("user_other_names"), qtTrId("lc-euvrc-user-other-names")},
        {QStringLiteral("user_address"), qtTrId("lc-euvrc-user-address")},
    };

    auto* section = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-euvrc-section-user"), userGroup, labels);

    // Hide when all fields are empty
    if (userGroup.fields.isEmpty())
        section->setVisible(false);

    return section;
}

CollapsibleSection* EuVrcWidget::buildNationalSection(const FieldGroup* group)
{
    // Translated labels for known Serbian national extension keys
    const std::map<QString, QString> knownLabels = {
        {QStringLiteral("owners_personal_no"), qtTrId("lc-euvrc-nat-owners-personal-no")},
        {QStringLiteral("users_personal_no"), qtTrId("lc-euvrc-nat-users-personal-no")},
        {QStringLiteral("vehicle_load"), qtTrId("lc-euvrc-nat-vehicle-load")},
        {QStringLiteral("year_of_production"), qtTrId("lc-euvrc-nat-year-of-production")},
        {QStringLiteral("serial_number"), qtTrId("lc-euvrc-nat-serial-number")},
    };

    FieldGroup natGroup;
    natGroup.key = QStringLiteral("national_display");

    std::map<QString, QString> labels;

    for (const auto& field : group->fields) {
        const QString val = field.value;
        if (!val.isEmpty()) {
            natGroup.fields.append(displayField(field.key, val));

            auto it = knownLabels.find(field.key);
            if (it != knownLabels.end()) {
                labels[field.key] = it->second;
            } else {
                QString displayLabel = field.extra.value(QStringLiteral("labelFallback")).toString();
                if (displayLabel.isEmpty())
                    displayLabel = field.key;
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
    // down the shell and rebuild from the cached groups.
    auto cachedGroups = std::move(groups);
    groups.clear();

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
