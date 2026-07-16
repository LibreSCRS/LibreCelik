// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "healthwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"

#include <plugin/carddatautils.h>

#include <QIcon>
#include <QToolButton>
#include <QVBoxLayout>

using librecelik::plugin::getFieldValue;

static std::map<std::string, QString> insuranceTranslationMap()
{
    return {
        {"insurer_name", qtTrId("lc-health-label-insurer")},
        {"insurer_id", qtTrId("lc-health-label-insurer-id")},
        {"card_id", qtTrId("lc-health-label-card-id")},
        {"date_of_issue", qtTrId("lc-health-label-issue-date")},
        {"date_of_expiry", qtTrId("lc-health-label-expiry")},
        {"valid_until", qtTrId("lc-health-label-valid-until")},
        {"permanently_valid", qtTrId("lc-health-label-permanently")},
        {"insurance_basis_rzzo", qtTrId("lc-health-label-insurance-basis")},
        {"insurance_description", qtTrId("lc-health-label-insurance-desc")},
        {"insurance_start_date", qtTrId("lc-health-label-insurance-start")},
    };
}

static std::map<std::string, QString> addressTranslationMap()
{
    return {
        {"street", qtTrId("lc-health-label-street")},
        {"address_number", qtTrId("lc-health-label-number")},
        {"apartment", qtTrId("lc-health-label-apartment")},
        {"place", qtTrId("lc-health-label-place")},
        {"municipality", qtTrId("lc-health-label-municipality")},
        {"country", qtTrId("lc-health-label-country")},
    };
}

static std::map<std::string, QString> carrierTranslationMap()
{
    return {
        {"carrier_given_name", qtTrId("lc-health-label-carrier-name")},
        {"carrier_family_name", qtTrId("lc-health-label-carrier-family-name")},
        {"carrier_relationship", qtTrId("lc-health-label-carrier-relation")},
        {"carrier_id_number", qtTrId("lc-health-label-carrier-id")},
        {"carrier_insurant_number", qtTrId("lc-health-label-carrier-lbo")},
        {"carrier_family_member", qtTrId("lc-health-label-family-member")},
    };
}

static std::map<std::string, QString> taxpayerTranslationMap()
{
    return {
        {"taxpayer_name", qtTrId("lc-health-label-taxpayer-name")},
        {"taxpayer_id_number", qtTrId("lc-health-label-taxpayer-id")},
        {"taxpayer_residence", qtTrId("lc-health-label-taxpayer-res")},
        {"taxpayer_activity_code", qtTrId("lc-health-label-taxpayer-act")},
    };
}

HealthWidget::HealthWidget(const LibreSCRS::Plugin::CardData& cardData, QWidget* parent) : HealthWidget(parent)
{
    data.cardType = cardData.cardType;
    for (const auto& group : cardData.groups)
        addGroup(group);
}

HealthWidget::HealthWidget(QWidget* parent) : plugin_ui::PluginWidgetBase(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    buildEmptyShell();
}

void HealthWidget::buildEmptyShell()
{
    static const QColor navy(34, 86, 117);

    outerSection = new CollapsibleSection(qtTrId("lc-health-title"), navy, this);
    outerSection->setHeaderHeight(56);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    contentLayout = new QVBoxLayout();
    outerSection->setLayout(contentLayout);

    outerLayout->addWidget(outerSection);

    // Print button — disabled until all data arrives
    printBtn = iconutils::createPrinterHeaderButton(this);
    connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(data); });
    outerSection->addHeaderWidget(printBtn);
}

void HealthWidget::addGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // Accumulate the raw group for the cardData() accessor, printing, and
    // retranslate rebuilds. Presentation transforms (e.g. localizing
    // permanently_valid) run on a render-only copy, never on this
    // source-of-truth.
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

void HealthWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

void HealthWidget::addPersonalGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // Build CardHeaderCard with health icon and key personal fields.
    // Insurance fields are not yet available, so only personal fields appear in header.
    std::vector<librecelik::utils::HeaderField> headerFields;
    headerFields.push_back({qtTrId("lc-health-label-given-name"), getFieldValue(&group, "given_name")});
    headerFields.push_back({qtTrId("lc-health-label-family-name"), getFieldValue(&group, "family_name")});
    headerFields.push_back({qtTrId("lc-health-label-jmbg"), getFieldValue(&group, "personal_number")});
    headerFields.push_back({qtTrId("lc-health-label-lbo"), getFieldValue(&group, "insurant_number")});

    QIcon healthIcon(QStringLiteral(":/images/health-icon.svg"));
    auto* headerCard = new librecelik::utils::CardHeaderCard(healthIcon, QSize(80, 80), headerFields, outerSection);
    contentLayout->addWidget(headerCard);
}

void HealthWidget::addInsuranceGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    // Localize permanently_valid (true/false -> Yes/No) on a render-only copy;
    // data.groups stays raw so printing and language switches see the card
    // data, not the presentation string.
    auto displayGroup = group;
    transformPermanentlyValid(displayGroup);

    auto* insuranceSec = librecelik::utils::FieldSectionBuilder::build(
        qtTrId("lc-health-section-insurance"), displayGroup, insuranceTranslationMap(), {}, outerSection);
    contentLayout->addWidget(insuranceSec);
}

void HealthWidget::addAddressGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    auto* addressSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-health-section-address"), group,
                                                                     addressTranslationMap(), {}, outerSection);
    contentLayout->addWidget(addressSec);
}

void HealthWidget::addCarrierGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    auto familyMember = getFieldValue(&group, "carrier_family_member");
    bool hasData = !group.fields.empty();
    bool showCarrier = hasData || familyMember == "true";

    if (showCarrier) {
        auto* carrierSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-health-section-carrier"), group,
                                                                             carrierTranslationMap(), {}, outerSection);
        contentLayout->addWidget(carrierSection);
    }
}

void HealthWidget::addTaxpayerGroup(const LibreSCRS::Plugin::CardFieldGroup& group)
{
    auto* taxpayerSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-health-section-taxpayer"), group,
                                                                      taxpayerTranslationMap(), {}, outerSection);
    contentLayout->addWidget(taxpayerSec);
}

void HealthWidget::transformPermanentlyValid(LibreSCRS::Plugin::CardFieldGroup& group)
{
    for (auto& field : group.fields) {
        if (field.key == "permanently_valid") {
            auto val = field.textValue();
            if (val.has_value() && *val == "true") {
                std::string yes = qtTrId("lc-health-val-yes").toStdString();
                field.value.assign(yes.begin(), yes.end());
            } else if (val.has_value() && *val == "false") {
                std::string no = qtTrId("lc-health-val-no").toStdString();
                field.value.assign(no.begin(), no.end());
            }
            break;
        }
    }
}

void HealthWidget::retranslateUi()
{
    // Plugin widget rebuild-tier (April 2026 retranslate spec): tear down the
    // shell and rebuild from the raw groups. data.groups is the immutable
    // source-of-truth (presentation transforms run on render-only copies), so
    // it reproduces correctly in the newly selected language.
    auto cachedGroups = std::move(data.groups);
    data.groups.clear();

    if (outerSection) {
        outerLayout->removeWidget(outerSection);
        outerSection->deleteLater();
        outerSection = nullptr;
    }
    contentLayout = nullptr;
    printBtn = nullptr;

    buildEmptyShell();
    for (const auto& group : cachedGroups)
        addGroup(group);
}
