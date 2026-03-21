// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "vehiclewidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"

#include <plugin/carddatautils.h>

#include <QDate>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

using plugin::getFieldValue;

VehicleWidget::VehicleWidget(const plugin::CardData& cardData, QWidget* parent) : QWidget(parent), data(cardData)
{
    buildLayout();
}

void VehicleWidget::buildLayout()
{
    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Navy outer CollapsibleSection
    auto* outerSection = new CollapsibleSection(qtTrId("lc-vehicle-title"), QColor(34, 86, 117), this);
    outerSection->setHeaderHeight(56);

    auto* contentLayout = new QVBoxLayout();
    contentLayout->setContentsMargins(8, 8, 8, 8);
    contentLayout->setSpacing(8);

    const auto* vehicleGroup = data.findGroup("vehicle");
    const auto* ownerGroup = data.findGroup("owner");
    const auto* userGroup = data.findGroup("user");

    // CardHeaderCard: icon + key fields
    if (vehicleGroup) {
        auto regNumber = getFieldValue(vehicleGroup, "registration_number");
        auto make = getFieldValue(vehicleGroup, "vehicle_make");
        auto model = getFieldValue(vehicleGroup, "commercial_description");
        auto year = getFieldValue(vehicleGroup, "year_of_production");
        auto expiry = getFieldValue(vehicleGroup, "expiry_date");

        QString ownerName;
        const auto* ownerSrc = ownerGroup ? ownerGroup : vehicleGroup;
        ownerName = getFieldValue(ownerSrc, "owners_surname_or_business_name");
        auto firstName = getFieldValue(ownerSrc, "owner_name");
        if (!firstName.isEmpty()) {
            if (!ownerName.isEmpty())
                ownerName += " ";
            ownerName += firstName;
        }

        std::vector<LibreSCRS::HeaderField> headerFields = {
            {"Registration", regNumber, 2}, {"Make", make},          {"Model", model}, {"Year", year},
            {"Owner", ownerName},           {"Valid to", expiry, 2},
        };

        auto* header = new LibreSCRS::CardHeaderCard(QIcon(), QSize(80, 80), headerFields, outerSection);
        contentLayout->addWidget(header);
    }

    // Engine, Mass, Capacity, Document — stacked vertically
    if (vehicleGroup) {
        contentLayout->addWidget(buildEngineSection(vehicleGroup));
        contentLayout->addWidget(buildMassSection(vehicleGroup));
        contentLayout->addWidget(buildCapacitySection(vehicleGroup));
        contentLayout->addWidget(buildDocumentSection(vehicleGroup));
    }

    // Owner/User (full width, hidden when all user fields empty)
    {
        const auto* ownerSrc = ownerGroup ? ownerGroup : vehicleGroup;
        const auto* userSrc = userGroup ? userGroup : vehicleGroup;
        if (ownerSrc || userSrc) {
            auto* ownerUserSection = buildOwnerUserSection(ownerSrc, userSrc);
            contentLayout->addWidget(ownerUserSection);
        }
    }

    outerSection->setLayout(contentLayout);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    outerLayout->addWidget(outerSection);
}

CollapsibleSection* VehicleWidget::buildEngineSection(const plugin::CardFieldGroup* group)
{
    plugin::CardFieldGroup engineGroup;
    engineGroup.groupKey = "engine";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty())
            engineGroup.fields.push_back({key, std::string(val.toUtf8().constData())});
    };

    addIfPresent("engine_id_number");
    addIfPresent("engine_capacity");
    addIfPresent("maximum_net_power");
    addIfPresent("type_of_fuel");

    std::map<std::string, QString> labels = {
        {"engine_id_number", "Engine number"},
        {"engine_capacity", "Capacity (cm3)"},
        {"maximum_net_power", "Power (kW)"},
        {"type_of_fuel", "Fuel type"},
    };

    return LibreSCRS::FieldSectionBuilder::build("Engine", engineGroup, labels);
}

CollapsibleSection* VehicleWidget::buildMassSection(const plugin::CardFieldGroup* group)
{
    plugin::CardFieldGroup massGroup;
    massGroup.groupKey = "mass";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty())
            massGroup.fields.push_back({key, std::string(val.toUtf8().constData())});
    };

    addIfPresent("vehicle_mass");
    addIfPresent("maximum_permissible_laden_mass");
    addIfPresent("vehicle_load");
    addIfPresent("power_weight_ratio");
    addIfPresent("number_of_axes");

    std::map<std::string, QString> labels = {
        {"vehicle_mass", "Mass (kg)"},         {"maximum_permissible_laden_mass", "Max laden mass (kg)"},
        {"vehicle_load", "Load (kg)"},         {"power_weight_ratio", "Power/weight ratio"},
        {"number_of_axes", "Number of axles"},
    };

    return LibreSCRS::FieldSectionBuilder::build("Mass and load", massGroup, labels);
}

CollapsibleSection* VehicleWidget::buildCapacitySection(const plugin::CardFieldGroup* group)
{
    plugin::CardFieldGroup capGroup;
    capGroup.groupKey = "capacity";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty())
            capGroup.fields.push_back({key, std::string(val.toUtf8().constData())});
    };

    addIfPresent("vehicle_category");
    addIfPresent("colour_of_vehicle");
    addIfPresent("number_of_seats");
    addIfPresent("number_of_standing_places");
    addIfPresent("vehicle_id_number");
    addIfPresent("vehicle_type");

    std::map<std::string, QString> labels = {
        {"vehicle_category", "Category"}, {"colour_of_vehicle", "Colour"},
        {"number_of_seats", "Seats"},     {"number_of_standing_places", "Standing places"},
        {"vehicle_id_number", "VIN"},     {"vehicle_type", "Type"},
    };

    return LibreSCRS::FieldSectionBuilder::build("Capacity", capGroup, labels);
}

CollapsibleSection* VehicleWidget::buildDocumentSection(const plugin::CardFieldGroup* group)
{
    plugin::CardFieldGroup docGroup;
    docGroup.groupKey = "document";

    auto addIfPresent = [&](const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty())
            docGroup.fields.push_back({key, std::string(val.toUtf8().constData())});
    };

    addIfPresent("state_issuing");
    addIfPresent("competent_authority");
    addIfPresent("authority_issuing");
    addIfPresent("unambiguous_number");
    addIfPresent("serial_number");
    addIfPresent("type_approval_number");
    addIfPresent("date_of_first_registration");
    addIfPresent("issuing_date");
    addIfPresent("expiry_date");

    std::map<std::string, QString> labels = {
        {"state_issuing", "State issuing"},
        {"competent_authority", "Competent authority"},
        {"authority_issuing", "Authority issuing"},
        {"unambiguous_number", "Unambiguous number"},
        {"serial_number", "Serial number"},
        {"type_approval_number", "Type approval number"},
        {"date_of_first_registration", "First registration"},
        {"issuing_date", "Date of issuance"},
        {"expiry_date", "Valid to"},
    };

    auto* section = LibreSCRS::FieldSectionBuilder::build("Document data", docGroup, labels);

    // Apply orange highlight to expiry date if expired
    auto expiryDate = getFieldValue(group, "expiry_date");
    QDate receivedDate = QDate::fromString(expiryDate, "dd.MM.yyyy");
    if (receivedDate.isValid() && receivedDate < QDate::currentDate()) {
        auto lineEdits = section->findChildren<QLineEdit*>();
        for (auto* edit : lineEdits) {
            if (edit->text() == expiryDate) {
                QPalette palette = edit->palette();
                palette.setColor(QPalette::Text, QColor(0xe6, 0x87, 0x3c));
                edit->setPalette(palette);
                break;
            }
        }
    }

    return section;
}

CollapsibleSection* VehicleWidget::buildOwnerUserSection(const plugin::CardFieldGroup* ownerGroup,
                                                         const plugin::CardFieldGroup* userGroup)
{
    plugin::CardFieldGroup combinedGroup;
    combinedGroup.groupKey = "owner_user";

    auto addIfPresent = [&](const plugin::CardFieldGroup* group, const char* key) {
        auto val = getFieldValue(group, key);
        if (!val.isEmpty())
            combinedGroup.fields.push_back({key, std::string(val.toUtf8().constData())});
    };

    // Owner fields
    if (ownerGroup) {
        addIfPresent(ownerGroup, "owners_surname_or_business_name");
        addIfPresent(ownerGroup, "owner_name");
        addIfPresent(ownerGroup, "owner_address");
        addIfPresent(ownerGroup, "owners_personal_no");
    }

    // User fields
    if (userGroup) {
        addIfPresent(userGroup, "users_surname_or_business_name");
        addIfPresent(userGroup, "users_name");
        addIfPresent(userGroup, "users_address");
        addIfPresent(userGroup, "users_personal_no");
    }

    std::map<std::string, QString> labels = {
        {"owners_surname_or_business_name", "Owner surname / Business name"},
        {"owner_name", "Owner name"},
        {"owner_address", "Owner address"},
        {"owners_personal_no", "Owner personal number"},
        {"users_surname_or_business_name", "User surname / Business name"},
        {"users_name", "User name"},
        {"users_address", "User address"},
        {"users_personal_no", "User personal number"},
    };

    auto* section = LibreSCRS::FieldSectionBuilder::build("Owner / User", combinedGroup, labels);

    // Hide when all fields are empty (combinedGroup would have no fields)
    if (combinedGroup.fields.empty()) {
        section->setVisible(false);
    }

    return section;
}
