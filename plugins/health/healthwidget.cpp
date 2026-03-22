// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "healthwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"

#include <plugin/carddatautils.h>

#include <QIcon>
#include <QVBoxLayout>

using plugin::getFieldValue;

// Translation maps: field key -> readable English fallback label.
// Final translations will be added in Task 10 via translations_catalog.cpp.

static const std::map<std::string, QString>& insuranceTranslationMap()
{
    static const std::map<std::string, QString> map = {
        {"insurer_name", "Insurer"},
        {"insurer_id", "Insurer ID"},
        {"card_id", "Card ID"},
        {"date_of_issue", "Issue date"},
        {"date_of_expiry", "Expiry date"},
        {"valid_until", "Valid until"},
        {"permanently_valid", "Permanently valid"},
        {"insurance_basis_rzzo", "Insurance basis"},
        {"insurance_description", "Insurance description"},
        {"insurance_start_date", "Insurance start"},
    };
    return map;
}

static const std::map<std::string, QString>& addressTranslationMap()
{
    static const std::map<std::string, QString> map = {
        {"street", "Street"}, {"address_number", "Number"},     {"apartment", "Apartment"},
        {"place", "Place"},   {"municipality", "Municipality"}, {"country", "Country"},
    };
    return map;
}

static const std::map<std::string, QString>& carrierTranslationMap()
{
    static const std::map<std::string, QString> map = {
        {"carrier_given_name", "Carrier name"},     {"carrier_family_name", "Carrier surname"},
        {"carrier_relationship", "Relationship"},   {"carrier_id_number", "Carrier ID"},
        {"carrier_insurant_number", "Carrier LBO"}, {"carrier_family_member", "Family member"},
    };
    return map;
}

static const std::map<std::string, QString>& taxpayerTranslationMap()
{
    static const std::map<std::string, QString> map = {
        {"taxpayer_name", "Employer name"},
        {"taxpayer_id_number", "Employer ID (PIB)"},
        {"taxpayer_residence", "Employer residence"},
        {"taxpayer_activity_code", "Activity code"},
    };
    return map;
}

HealthWidget::HealthWidget(const plugin::CardData& cardData, QWidget* parent) : QWidget(parent), data(cardData)
{
    transformPermanentlyValid();
    buildLayout();
}

HealthWidget::HealthWidget(QWidget* parent) : QWidget(parent)
{
    buildEmptyShell();
}

void HealthWidget::buildEmptyShell()
{
    static const QColor navy(34, 86, 117);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    outerSection = new CollapsibleSection(qtTrId("lc-health-title"), navy, this);
    outerSection->setHeaderHeight(56);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    contentLayout = new QVBoxLayout();
    outerSection->setLayout(contentLayout);

    outerLayout->addWidget(outerSection);
}

void HealthWidget::addGroup(const plugin::CardFieldGroup& group)
{
    // Accumulate into data for cardData() accessor and printing
    data.groups.push_back(group);

    const auto& key = group.groupKey;
    if (key == "personal") {
        addPersonalGroup(group);
    } else if (key == "insurance") {
        addInsuranceGroup(group);
    } else if (key == "address") {
        addAddressGroup(group);
    } else if (key == "carrier") {
        addCarrierGroup(group);
    } else if (key == "taxpayer") {
        addTaxpayerGroup(group);
    }
}

void HealthWidget::addPersonalGroup(const plugin::CardFieldGroup& group)
{
    // Build CardHeaderCard with health icon and key personal fields.
    // Insurance fields are not yet available, so only personal fields appear in header.
    std::vector<LibreSCRS::HeaderField> headerFields;
    headerFields.push_back({"Name", getFieldValue(&group, "given_name")});
    headerFields.push_back({"Surname", getFieldValue(&group, "family_name")});
    headerFields.push_back({"Personal number (JMBG)", getFieldValue(&group, "personal_number")});
    headerFields.push_back({"Insurant number (LBO)", getFieldValue(&group, "insurant_number")});

    QIcon healthIcon(QStringLiteral(":/images/health-icon.svg"));
    auto* headerCard = new LibreSCRS::CardHeaderCard(healthIcon, QSize(80, 80), headerFields, outerSection);
    contentLayout->addWidget(headerCard);
}

void HealthWidget::addInsuranceGroup(const plugin::CardFieldGroup& group)
{
    // Transform permanently_valid in accumulated data
    transformPermanentlyValid(data.groups.back());

    auto* insuranceSec = LibreSCRS::FieldSectionBuilder::build("Insurance", data.groups.back(),
                                                               insuranceTranslationMap(), {}, outerSection);
    contentLayout->addWidget(insuranceSec);
}

void HealthWidget::addAddressGroup(const plugin::CardFieldGroup& group)
{
    auto* addressSec =
        LibreSCRS::FieldSectionBuilder::build("Address", group, addressTranslationMap(), {}, outerSection);
    contentLayout->addWidget(addressSec);
}

void HealthWidget::addCarrierGroup(const plugin::CardFieldGroup& group)
{
    auto familyMember = getFieldValue(&group, "carrier_family_member");
    bool hasData = !group.fields.empty();
    bool showCarrier = hasData || familyMember == "true";

    if (showCarrier) {
        carrierSection = LibreSCRS::FieldSectionBuilder::build("Insurance Carrier", group, carrierTranslationMap(), {},
                                                               outerSection);
        contentLayout->addWidget(carrierSection);
    }
}

void HealthWidget::addTaxpayerGroup(const plugin::CardFieldGroup& group)
{
    auto* taxpayerSec =
        LibreSCRS::FieldSectionBuilder::build("Employer / Taxpayer", group, taxpayerTranslationMap(), {}, outerSection);
    contentLayout->addWidget(taxpayerSec);
}

void HealthWidget::transformPermanentlyValid()
{
    if (auto* field = data.findField("permanently_valid")) {
        auto val = field->asString();
        if (val == "true") {
            std::string yes = qtTrId("lc-health-val-yes").toStdString();
            field->value.assign(yes.begin(), yes.end());
        } else if (val == "false") {
            std::string no = qtTrId("lc-health-val-no").toStdString();
            field->value.assign(no.begin(), no.end());
        }
    }
}

void HealthWidget::transformPermanentlyValid(plugin::CardFieldGroup& group)
{
    for (auto& field : group.fields) {
        if (field.key == "permanently_valid") {
            auto val = field.asString();
            if (val == "true") {
                std::string yes = qtTrId("lc-health-val-yes").toStdString();
                field.value.assign(yes.begin(), yes.end());
            } else if (val == "false") {
                std::string no = qtTrId("lc-health-val-no").toStdString();
                field.value.assign(no.begin(), no.end());
            }
            break;
        }
    }
}

void HealthWidget::buildLayout()
{
    static const QColor navy(34, 86, 117);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Outer navy CollapsibleSection
    auto* outerSection = new CollapsibleSection(qtTrId("lc-health-title"), navy, this);
    outerSection->setHeaderHeight(56);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    auto* contentLayout = new QVBoxLayout();

    // --- CardHeaderCard with health icon and key personal fields ---
    const auto* personal = data.findGroup("personal");
    const auto* insurance = data.findGroup("insurance");

    std::vector<LibreSCRS::HeaderField> headerFields;
    if (personal) {
        headerFields.push_back({"Name", getFieldValue(personal, "given_name")});
        headerFields.push_back({"Surname", getFieldValue(personal, "family_name")});
        headerFields.push_back({"Personal number (JMBG)", getFieldValue(personal, "personal_number")});
        headerFields.push_back({"Insurant number (LBO)", getFieldValue(personal, "insurant_number")});
    }
    if (insurance) {
        headerFields.push_back({"Card ID", getFieldValue(insurance, "card_id")});
        headerFields.push_back({"Valid until", getFieldValue(insurance, "valid_until")});
    }

    QIcon healthIcon(QStringLiteral(":/images/health-icon.svg"));
    auto* headerCard = new LibreSCRS::CardHeaderCard(healthIcon, QSize(80, 80), headerFields, outerSection);
    contentLayout->addWidget(headerCard);

    // --- Insurance, Address — stacked vertically ---
    if (insurance) {
        auto* insuranceSec =
            LibreSCRS::FieldSectionBuilder::build("Insurance", *insurance, insuranceTranslationMap(), {}, outerSection);
        contentLayout->addWidget(insuranceSec);
    }
    if (const auto* address = data.findGroup("address")) {
        auto* addressSec =
            LibreSCRS::FieldSectionBuilder::build("Address", *address, addressTranslationMap(), {}, outerSection);
        contentLayout->addWidget(addressSec);
    }

    // --- Carrier, Taxpayer — stacked vertically ---
    const auto* carrier = data.findGroup("carrier");
    bool showCarrier = false;
    if (carrier) {
        auto familyMember = getFieldValue(carrier, "carrier_family_member");
        bool hasData = !carrier->fields.empty();
        showCarrier = hasData || familyMember == "true";
    }

    if (showCarrier) {
        carrierSection = LibreSCRS::FieldSectionBuilder::build("Insurance Carrier", *carrier, carrierTranslationMap(),
                                                               {}, outerSection);
        contentLayout->addWidget(carrierSection);
    }

    if (const auto* taxpayer = data.findGroup("taxpayer")) {
        auto* taxpayerSec = LibreSCRS::FieldSectionBuilder::build("Employer / Taxpayer", *taxpayer,
                                                                  taxpayerTranslationMap(), {}, outerSection);
        contentLayout->addWidget(taxpayerSec);
    }

    outerSection->setLayout(contentLayout);

    outerLayout->addWidget(outerSection);
}
