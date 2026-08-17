// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "healthwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"

#include <plugin/fieldvalue.h>

#include <QIcon>
#include <QToolButton>
#include <QVBoxLayout>

using librecelik::plugin::fieldValue;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

static std::map<QString, QString> insuranceTranslationMap()
{
    return {
        {QStringLiteral("insurer_name"), qtTrId("lc-health-label-insurer")},
        {QStringLiteral("insurer_id"), qtTrId("lc-health-label-insurer-id")},
        {QStringLiteral("card_id"), qtTrId("lc-health-label-card-id")},
        {QStringLiteral("date_of_issue"), qtTrId("lc-health-label-issue-date")},
        {QStringLiteral("date_of_expiry"), qtTrId("lc-health-label-expiry")},
        {QStringLiteral("valid_until"), qtTrId("lc-health-label-valid-until")},
        {QStringLiteral("permanently_valid"), qtTrId("lc-health-label-permanently")},
        {QStringLiteral("insurance_basis_rzzo"), qtTrId("lc-health-label-insurance-basis")},
        {QStringLiteral("insurance_description"), qtTrId("lc-health-label-insurance-desc")},
        {QStringLiteral("insurance_start_date"), qtTrId("lc-health-label-insurance-start")},
    };
}

static std::map<QString, QString> addressTranslationMap()
{
    return {
        {QStringLiteral("street"), qtTrId("lc-health-label-street")},
        {QStringLiteral("address_number"), qtTrId("lc-health-label-number")},
        {QStringLiteral("apartment"), qtTrId("lc-health-label-apartment")},
        {QStringLiteral("place"), qtTrId("lc-health-label-place")},
        {QStringLiteral("municipality"), qtTrId("lc-health-label-municipality")},
        {QStringLiteral("country"), qtTrId("lc-health-label-country")},
    };
}

static std::map<QString, QString> carrierTranslationMap()
{
    return {
        {QStringLiteral("carrier_given_name"), qtTrId("lc-health-label-carrier-name")},
        {QStringLiteral("carrier_family_name"), qtTrId("lc-health-label-carrier-family-name")},
        {QStringLiteral("carrier_relationship"), qtTrId("lc-health-label-carrier-relation")},
        {QStringLiteral("carrier_id_number"), qtTrId("lc-health-label-carrier-id")},
        {QStringLiteral("carrier_insurant_number"), qtTrId("lc-health-label-carrier-lbo")},
        {QStringLiteral("carrier_family_member"), qtTrId("lc-health-label-family-member")},
    };
}

static std::map<QString, QString> taxpayerTranslationMap()
{
    return {
        {QStringLiteral("taxpayer_name"), qtTrId("lc-health-label-taxpayer-name")},
        {QStringLiteral("taxpayer_id_number"), qtTrId("lc-health-label-taxpayer-id")},
        {QStringLiteral("taxpayer_residence"), qtTrId("lc-health-label-taxpayer-res")},
        {QStringLiteral("taxpayer_activity_code"), qtTrId("lc-health-label-taxpayer-act")},
    };
}

HealthWidget::HealthWidget(const QList<FieldGroup>& cardGroups, QWidget* parent) : HealthWidget(parent)
{
    // Staged into the widget's own section order — the final wire model's
    // group order is delivery-dependent (see stagedForBuild).
    for (const auto& group : librecelik::plugin::stagedForBuild(
             cardGroups, {u"personal", u"insurance", u"address", u"carrier", u"taxpayer"}))
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
    connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(groups); });
    outerSection->addHeaderWidget(printBtn);
}

void HealthWidget::addGroup(const FieldGroup& group)
{
    // Accumulate the raw group for the fieldGroups() accessor, printing, and
    // retranslate rebuilds. Presentation transforms (e.g. localizing
    // permanently_valid) run on a render-only copy, never on this
    // source-of-truth.
    groups.append(group);

    const QString key = group.key;
    if (key == QLatin1String("personal")) {
        addPersonalGroup(group);
    } else if (key == QLatin1String("insurance")) {
        addInsuranceGroup(group);
    } else if (key == QLatin1String("address")) {
        addAddressGroup(group);
    } else if (key == QLatin1String("carrier")) {
        addCarrierGroup(group);
    } else if (key == QLatin1String("taxpayer")) {
        addTaxpayerGroup(group);
    }
}

void HealthWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

void HealthWidget::addPersonalGroup(const FieldGroup& group)
{
    // Build CardHeaderCard with health icon and key personal fields.
    // Insurance fields are not yet available, so only personal fields appear in header.
    std::vector<librecelik::utils::HeaderField> headerFields;
    headerFields.push_back({qtTrId("lc-health-label-given-name"), fieldValue(group, u"given_name")});
    headerFields.push_back({qtTrId("lc-health-label-family-name"), fieldValue(group, u"family_name")});
    headerFields.push_back({qtTrId("lc-health-label-jmbg"), fieldValue(group, u"personal_number")});
    headerFields.push_back({qtTrId("lc-health-label-lbo"), fieldValue(group, u"insurant_number")});

    QIcon healthIcon(QStringLiteral(":/images/health-icon.svg"));
    auto* headerCard = new librecelik::utils::CardHeaderCard(healthIcon, QSize(80, 80), headerFields, outerSection);
    contentLayout->addWidget(headerCard);
}

void HealthWidget::addInsuranceGroup(const FieldGroup& group)
{
    // Localize permanently_valid (true/false -> Yes/No) on a render-only copy;
    // groups stays raw so printing and language switches see the card data,
    // not the presentation string.
    auto displayGroup = group;
    transformPermanentlyValid(displayGroup);

    auto* insuranceSec = librecelik::utils::FieldSectionBuilder::build(
        qtTrId("lc-health-section-insurance"), displayGroup, insuranceTranslationMap(), {}, outerSection);
    contentLayout->addWidget(insuranceSec);
}

void HealthWidget::addAddressGroup(const FieldGroup& group)
{
    auto* addressSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-health-section-address"), group,
                                                                     addressTranslationMap(), {}, outerSection);
    contentLayout->addWidget(addressSec);
}

void HealthWidget::addCarrierGroup(const FieldGroup& group)
{
    auto familyMember = fieldValue(group, u"carrier_family_member");
    bool hasData = !group.fields.isEmpty();
    bool showCarrier = hasData || familyMember == QLatin1String("true");

    if (showCarrier) {
        auto* carrierSection = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-health-section-carrier"), group,
                                                                             carrierTranslationMap(), {}, outerSection);
        contentLayout->addWidget(carrierSection);
    }
}

void HealthWidget::addTaxpayerGroup(const FieldGroup& group)
{
    auto* taxpayerSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-health-section-taxpayer"), group,
                                                                      taxpayerTranslationMap(), {}, outerSection);
    contentLayout->addWidget(taxpayerSec);
}

void HealthWidget::transformPermanentlyValid(FieldGroup& group)
{
    for (Field& field : group.fields) {
        if (field.key == QLatin1String("permanently_valid")) {
            if (field.value == QLatin1String("true")) {
                field.value = qtTrId("lc-health-val-yes");
            } else if (field.value == QLatin1String("false")) {
                field.value = qtTrId("lc-health-val-no");
            }
            break;
        }
    }
}

void HealthWidget::retranslateUi()
{
    // Plugin widget rebuild-tier (April 2026 retranslate spec): tear down the
    // shell and rebuild from the raw groups. groups is the immutable
    // source-of-truth (presentation transforms run on render-only copies), so
    // it reproduces correctly in the newly selected language.
    auto cachedGroups = std::move(groups);
    groups.clear();

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
