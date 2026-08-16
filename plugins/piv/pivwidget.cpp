// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pivwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"

#include <plugin/fieldvalue.h>

#include <QToolButton>
#include <QVBoxLayout>

using librecelik::plugin::fieldValue;
using LibreSCRS::AgentClient::FieldGroup;

static std::map<QString, QString> chuidTranslationMap()
{
    return {
        {QStringLiteral("guid"), qtTrId("lc-piv-field-guid")},
        {QStringLiteral("fascn"), qtTrId("lc-piv-field-fascn")},
        {QStringLiteral("expirationDate"), qtTrId("lc-piv-field-expiration")},
    };
}

static std::map<QString, QString> cccTranslationMap()
{
    return {
        {QStringLiteral("cardIdentifier"), qtTrId("lc-piv-field-cardid")},
    };
}

static std::map<QString, QString> printedTranslationMap()
{
    return {
        {QStringLiteral("name"), qtTrId("lc-piv-field-name")},
        {QStringLiteral("employeeAffiliation"), qtTrId("lc-piv-field-affiliation")},
        {QStringLiteral("org1"), qtTrId("lc-piv-field-org1")},
        {QStringLiteral("org2"), qtTrId("lc-piv-field-org2")},
        {QStringLiteral("expiry"), qtTrId("lc-piv-field-expiration")},
        {QStringLiteral("serialNumber"), qtTrId("lc-piv-field-serial")},
        {QStringLiteral("issuerId"), qtTrId("lc-piv-field-issuer")},
    };
}

static std::map<QString, QString> discoveryTranslationMap()
{
    return {
        {QStringLiteral("pinPolicy"), qtTrId("lc-piv-field-pinpolicy")},
    };
}

static std::map<QString, QString> keyHistoryTranslationMap()
{
    return {
        {QStringLiteral("onCardCerts"), qtTrId("lc-piv-field-oncardcerts")},
        {QStringLiteral("offCardCerts"), qtTrId("lc-piv-field-offcardcerts")},
        {QStringLiteral("offCardURL"), qtTrId("lc-piv-field-offcardurl")},
    };
}

PIVWidget::PIVWidget(const QList<FieldGroup>& cardGroups, QWidget* parent) : PIVWidget(parent)
{
    for (const auto& group : cardGroups)
        addGroup(group);
}

PIVWidget::PIVWidget(QWidget* parent) : plugin_ui::PluginWidgetBase(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    buildEmptyShell();
}

void PIVWidget::buildEmptyShell()
{
    static const QColor pivColor(61, 90, 128);

    outerSection = new CollapsibleSection(qtTrId("lc-piv-widget-title"), pivColor, this);
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

void PIVWidget::addGroup(const FieldGroup& group)
{
    groups.append(group);

    const QString key = group.key;
    if (key == QLatin1String("chuid")) {
        addChuidGroup(group);
    } else if (key == QLatin1String("ccc")) {
        addCccGroup(group);
    } else if (key == QLatin1String("printed")) {
        addPrintedGroup(group);
    } else if (key == QLatin1String("discovery")) {
        addDiscoveryGroup(group);
    } else if (key == QLatin1String("keyHistory")) {
        addKeyHistoryGroup(group);
    }
    // "pki" group is intentionally skipped — handled by TokenSection automatically
}

void PIVWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

void PIVWidget::addChuidGroup(const FieldGroup& group)
{
    // Build CardHeaderCard with PIV icon and CHUID key fields
    std::vector<librecelik::utils::HeaderField> headerFields;
    headerFields.push_back({qtTrId("lc-piv-field-guid"), fieldValue(group, u"guid")});
    headerFields.push_back({qtTrId("lc-piv-field-expiration"), fieldValue(group, u"expirationDate")});

    QIcon pivIcon(QStringLiteral(":/images/piv-icon.svg"));
    headerCard = new librecelik::utils::CardHeaderCard(pivIcon, QSize(80, 80), headerFields, outerSection);
    contentLayout->addWidget(headerCard);

    // CHUID details section
    auto* chuidSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-piv-section-chuid"), group,
                                                                   chuidTranslationMap(), {}, outerSection);
    contentLayout->addWidget(chuidSec);
}

void PIVWidget::addCccGroup(const FieldGroup& group)
{
    auto* cccSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-piv-section-ccc"), group,
                                                                 cccTranslationMap(), {}, outerSection);
    contentLayout->addWidget(cccSec);
}

void PIVWidget::addPrintedGroup(const FieldGroup& group)
{
    auto* printedSec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-piv-section-printed"), group,
                                                                     printedTranslationMap(), {}, outerSection);
    contentLayout->addWidget(printedSec);

    // Rebuild header with name if available
    auto name = fieldValue(group, u"name");
    if (!name.isEmpty())
        rebuildHeader();
}

void PIVWidget::addDiscoveryGroup(const FieldGroup& group)
{
    auto* discoverySec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-piv-section-discovery"), group,
                                                                       discoveryTranslationMap(), {}, outerSection);
    contentLayout->addWidget(discoverySec);
}

void PIVWidget::addKeyHistoryGroup(const FieldGroup& group)
{
    auto* keyHistorySec = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-piv-section-keyhistory"), group,
                                                                        keyHistoryTranslationMap(), {}, outerSection);
    contentLayout->addWidget(keyHistorySec);
}

void PIVWidget::rebuildHeader()
{
    if (!headerCard)
        return;

    // Rebuild with name from printed group as primary field
    auto name = fieldValue(groups, u"name");
    auto guid = fieldValue(groups, u"guid");
    auto expiration = fieldValue(groups, u"expirationDate");

    std::vector<librecelik::utils::HeaderField> headerFields;
    if (!name.isEmpty())
        headerFields.push_back({qtTrId("lc-piv-field-name"), name});
    headerFields.push_back({qtTrId("lc-piv-field-guid"), guid});
    headerFields.push_back({qtTrId("lc-piv-field-expiration"), expiration});

    auto* newHeader = new librecelik::utils::CardHeaderCard(QIcon(":/images/piv-icon.svg"), QSize(80, 80), headerFields,
                                                            outerSection);

    auto* item = contentLayout->replaceWidget(headerCard, newHeader);
    delete item;
    headerCard->deleteLater();
    headerCard = newHeader;
}

void PIVWidget::retranslateUi()
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

    buildEmptyShell();
    for (const auto& group : cachedGroups)
        addGroup(group);
}
