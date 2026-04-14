// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pivwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"

#include <plugin/carddatautils.h>

#include <QToolButton>
#include <QVBoxLayout>

using plugin::getFieldValue;

static std::map<std::string, QString> chuidTranslationMap()
{
    return {
        {"guid", qtTrId("lc-piv-field-guid")},
        {"fascn", qtTrId("lc-piv-field-fascn")},
        {"expirationDate", qtTrId("lc-piv-field-expiration")},
    };
}

static std::map<std::string, QString> cccTranslationMap()
{
    return {
        {"cardIdentifier", qtTrId("lc-piv-field-cardid")},
    };
}

static std::map<std::string, QString> printedTranslationMap()
{
    return {
        {"name", qtTrId("lc-piv-field-name")},         {"employeeAffiliation", qtTrId("lc-piv-field-affiliation")},
        {"org1", qtTrId("lc-piv-field-org1")},         {"org2", qtTrId("lc-piv-field-org2")},
        {"expiry", qtTrId("lc-piv-field-expiration")}, {"serialNumber", qtTrId("lc-piv-field-serial")},
        {"issuerId", qtTrId("lc-piv-field-issuer")},
    };
}

static std::map<std::string, QString> discoveryTranslationMap()
{
    return {
        {"pinPolicy", qtTrId("lc-piv-field-pinpolicy")},
    };
}

static std::map<std::string, QString> keyHistoryTranslationMap()
{
    return {
        {"onCardCerts", qtTrId("lc-piv-field-oncardcerts")},
        {"offCardCerts", qtTrId("lc-piv-field-offcardcerts")},
        {"offCardURL", qtTrId("lc-piv-field-offcardurl")},
    };
}

PIVWidget::PIVWidget(const plugin::CardData& cardData, QWidget* parent) : PIVWidget(parent){
    data.cardType = cardData.cardType;
    for (const auto& group : cardData.groups)
        addGroup(group);
}

PIVWidget::PIVWidget(QWidget* parent) : plugin_ui::PluginWidgetBase(parent)
{
    buildEmptyShell();
}

void PIVWidget::buildEmptyShell()
{
    static const QColor pivColor(61, 90, 128);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    outerSection = new CollapsibleSection(qtTrId("lc-piv-widget-title"), pivColor, this);
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

void PIVWidget::addGroup(const plugin::CardFieldGroup& group)
{
    data.groups.push_back(group);

    const auto& key = group.groupKey;
    if (key == "chuid") {
        addChuidGroup(group);
    } else if (key == "ccc") {
        addCccGroup(group);
    } else if (key == "printed") {
        addPrintedGroup(group);
    } else if (key == "discovery") {
        addDiscoveryGroup(group);
    } else if (key == "keyHistory") {
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

void PIVWidget::addChuidGroup(const plugin::CardFieldGroup& group)
{
    // Build CardHeaderCard with PIV icon and CHUID key fields
    std::vector<LibreSCRS::HeaderField> headerFields;
    headerFields.push_back({qtTrId("lc-piv-field-guid"), getFieldValue(&group, "guid")});
    headerFields.push_back({qtTrId("lc-piv-field-expiration"), getFieldValue(&group, "expirationDate")});

    QIcon pivIcon(QStringLiteral(":/images/piv-icon.svg"));
    headerCard = new LibreSCRS::CardHeaderCard(pivIcon, QSize(80, 80), headerFields, outerSection);
    contentLayout->addWidget(headerCard);

    // CHUID details section
    auto* chuidSec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-piv-section-chuid"), group, chuidTranslationMap(),
                                                           {}, outerSection);
    contentLayout->addWidget(chuidSec);
}

void PIVWidget::addCccGroup(const plugin::CardFieldGroup& group)
{
    auto* cccSec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-piv-section-ccc"), group, cccTranslationMap(), {},
                                                         outerSection);
    contentLayout->addWidget(cccSec);
}

void PIVWidget::addPrintedGroup(const plugin::CardFieldGroup& group)
{
    auto* printedSec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-piv-section-printed"), group,
                                                             printedTranslationMap(), {}, outerSection);
    contentLayout->addWidget(printedSec);

    // Rebuild header with name if available
    auto name = getFieldValue(&group, "name");
    if (!name.isEmpty())
        rebuildHeader();
}

void PIVWidget::addDiscoveryGroup(const plugin::CardFieldGroup& group)
{
    auto* discoverySec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-piv-section-discovery"), group,
                                                               discoveryTranslationMap(), {}, outerSection);
    contentLayout->addWidget(discoverySec);
}

void PIVWidget::addKeyHistoryGroup(const plugin::CardFieldGroup& group)
{
    auto* keyHistorySec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-piv-section-keyhistory"), group,
                                                                keyHistoryTranslationMap(), {}, outerSection);
    contentLayout->addWidget(keyHistorySec);
}

void PIVWidget::rebuildHeader()
{
    if (!headerCard)
        return;

    // Rebuild with name from printed group as primary field
    auto name = getFieldValue(data, "name");
    auto guid = getFieldValue(data, "guid");
    auto expiration = getFieldValue(data, "expirationDate");

    std::vector<LibreSCRS::HeaderField> headerFields;
    if (!name.isEmpty())
        headerFields.push_back({qtTrId("lc-piv-field-name"), name});
    headerFields.push_back({qtTrId("lc-piv-field-guid"), guid});
    headerFields.push_back({qtTrId("lc-piv-field-expiration"), expiration});

    auto* newHeader =
        new LibreSCRS::CardHeaderCard(QIcon(":/images/piv-icon.svg"), QSize(80, 80), headerFields, outerSection);

    auto* item = contentLayout->replaceWidget(headerCard, newHeader);
    delete item;
    headerCard->deleteLater();
    headerCard = newHeader;
}

void PIVWidget::retranslateUi()
{
    if (outerSection)
        outerSection->setTitle(qtTrId("lc-piv-widget-title"));
    if (printBtn)
        printBtn->setToolTip(qtTrId("lc-print-tooltip"));
}
