// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "eidwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"

#include <plugin/carddatautils.h>

#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

using plugin::getFieldValue;

EidWidget::EidWidget(const plugin::CardData& cardData, QWidget* parent) : EidWidget(parent)
{
    data.cardType = cardData.cardType;
    for (const auto& group : cardData.groups)
        addGroup(group);
    // Non-streaming: verification data may be in "meta" group rather than a
    // separate "verification" group. Apply if streaming didn't handle it.
    if (personalSection && !data.findGroup("verification")) {
        const auto* meta = data.findGroup("meta");
        if (meta)
            addVerificationBadges(personalSection, meta);
    }
}

EidWidget::EidWidget(QWidget* parent) : QWidget(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
}

void EidWidget::addGroup(const plugin::CardFieldGroup& group)
{
    data.groups.push_back(group);

    const auto key = QString::fromStdString(group.groupKey);

    if (key == QLatin1String("meta")) {
        bool foreigner = isForeigner();
        outerSection = new CollapsibleSection(foreigner ? qtTrId("lc-eid-title-foreigner") : qtTrId("lc-eid-title"),
                                              QColor(34, 86, 117), this);
        outerSection->setHeaderHeight(56);

        sectionLayout = new QVBoxLayout();
        sectionLayout->setSpacing(6);

        outerSection->setLayout(sectionLayout);
        outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        outerLayout->addWidget(outerSection);

        // Print button — disabled (dimmed) until all data arrives
        printBtn = new QToolButton(this);
        {
            QIcon icon(QStringLiteral(":/images/printer-header.svg"));
            auto normalPix = icon.pixmap(24, 24);
            QPixmap dimPix(normalPix.size());
            dimPix.fill(Qt::transparent);
            QPainter p(&dimPix);
            p.setOpacity(0.3);
            p.drawPixmap(0, 0, normalPix);
            p.end();
            icon.addPixmap(dimPix, QIcon::Disabled);
            printBtn->setIcon(icon);
        }
        printBtn->setIconSize(QSize(24, 24));
        printBtn->setToolTip(qtTrId("lc-print-tooltip"));
        printBtn->setAutoRaise(true);
        printBtn->setEnabled(false);
        connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(data); });
        outerSection->addHeaderWidget(printBtn);

    } else if (key == QLatin1String("personal")) {
        if (!outerSection)
            return;

        auto* photoRow = new QHBoxLayout();
        photoRow->setSpacing(10);

        photoLabel = new QLabel(outerSection);
        QPixmap placeholder(240, 320);
        placeholder.fill(QColor(220, 220, 220));
        {
            QPixmap userIcon(QStringLiteral(":/images/user.png"));
            auto scaled = userIcon.scaled(QSize(120, 160), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPainter painter(&placeholder);
            painter.drawPixmap((240 - scaled.width()) / 2, (320 - scaled.height()) / 2, scaled);
        }
        photoLabel->setPixmap(placeholder);
        photoLabel->setFixedSize(240, 320);
        photoRow->addWidget(photoLabel, 0, Qt::AlignTop);

        personalSection = buildPersonalSection(outerSection);
        addVerificationBadges(personalSection); // initial Unknown state
        photoRow->addWidget(personalSection, 1);

        sectionLayout->addLayout(photoRow);

    } else if (key == QLatin1String("address")) {
        if (!sectionLayout)
            return;
        sectionLayout->addWidget(buildAddressSection(outerSection));

    } else if (key == QLatin1String("document")) {
        if (!sectionLayout)
            return;
        sectionLayout->addWidget(buildDocumentSection(outerSection));

    } else if (key == QLatin1String("photo")) {
        if (!photoLabel)
            return;
        QPixmap photo = loadPhoto();
        photoLabel->setPixmap(photo);
        photoLabel->setFixedSize(photo.size());

    } else if (key == QLatin1String("verification")) {
        if (!personalSection)
            return;
        addVerificationBadges(personalSection, &group);
    }
}

void EidWidget::enablePrintButton()
{
    if (printBtn) {
        printBtn->setEnabled(true);
    }
}

bool EidWidget::isForeigner() const
{
    const auto* cardTypeField = data.findField("card_type");
    return cardTypeField && cardTypeField->asString() == "ForeignerIF2020";
}

QPixmap EidWidget::loadPhoto() const
{
    const auto* photoGroup = data.findGroup("photo");
    if (photoGroup && !photoGroup->fields.empty() && !photoGroup->fields[0].value.empty()) {
        QPixmap pixmap;
        pixmap.loadFromData(photoGroup->fields[0].value.data(), static_cast<uint>(photoGroup->fields[0].value.size()));
        if (!pixmap.isNull())
            return pixmap;
    }
    return QPixmap(QStringLiteral(":/images/user.png"));
}

CollapsibleSection* EidWidget::buildPersonalSection(QWidget* parent) const
{
    const auto* personal = data.findGroup("personal");
    if (!personal) {
        auto* section = new CollapsibleSection(qtTrId("lc-personal-data-title"), parent);
        section->setCollapsible(false);
        return section;
    }

    bool foreigner = isForeigner();

    std::map<std::string, QString> translationMap = {
        {"given_name", qtTrId("lc-eid-label-given-name")},
        {"surname", qtTrId("lc-eid-label-surname")},
        {"date_of_birth", qtTrId("lc-eid-label-date-of-birth")},
        {"personal_number", foreigner ? qtTrId("lc-eid-label-ebs") : qtTrId("lc-eid-label-jmbg")},
        {"sex", qtTrId("lc-eid-label-sex")},
    };

    std::set<std::string> hiddenFields = {
        "place_of_birth",
        "community_of_birth",
        "state_of_birth",
    };

    if (foreigner) {
        translationMap["nationality"] = qtTrId("lc-eid-label-nationality");
        translationMap["status_of_foreigner"] = qtTrId("lc-eid-label-status-of-foreigner");
        hiddenFields.insert("parent_given_name");
    } else {
        translationMap["parent_given_name"] = qtTrId("lc-eid-label-parent-name");
        hiddenFields.insert("nationality");
        hiddenFields.insert("status_of_foreigner");
    }

    // Synthetic composite place-of-birth for foreigners
    plugin::CardFieldGroup modifiedGroup = *personal;
    if (foreigner) {
        QStringList pobParts;
        pobParts << getFieldValue(personal, "place_of_birth") << getFieldValue(personal, "community_of_birth")
                 << getFieldValue(personal, "state_of_birth");
        pobParts.removeAll(QString());
        auto pobValue = pobParts.join(", ").toStdString();
        if (!pobValue.empty()) {
            modifiedGroup.fields.push_back({"place_of_birth_composite",
                                            "Place of Birth",
                                            plugin::FieldType::Text,
                                            {pobValue.begin(), pobValue.end()}});
            translationMap["place_of_birth_composite"] = qtTrId("lc-eid-label-place-of-birth");
        }
    }

    auto* section = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-personal-data-title"), modifiedGroup,
                                                          translationMap, hiddenFields, parent);
    section->setCollapsible(false);
    return section;
}

void EidWidget::addVerificationBadges(CollapsibleSection* section, const plugin::CardFieldGroup* source)
{
    if (!section)
        return;

    auto toResult = [](const QString& val) -> LibreSCRS::VerificationStatus::Result {
        if (val == "valid")
            return LibreSCRS::VerificationStatus::Valid;
        if (val == "invalid")
            return LibreSCRS::VerificationStatus::Invalid;
        return LibreSCRS::VerificationStatus::Unknown;
    };

    std::vector<LibreSCRS::VerificationStatus> results = {
        {qtTrId("lc-eid-label-card-verification"),
         source ? toResult(getFieldValue(source, "card_verification")) : LibreSCRS::VerificationStatus::Unknown},
        {qtTrId("lc-eid-label-fixed-verification"),
         source ? toResult(getFieldValue(source, "fixed_verification")) : LibreSCRS::VerificationStatus::Unknown},
        {qtTrId("lc-eid-label-variable-verification"),
         source ? toResult(getFieldValue(source, "variable_verification")) : LibreSCRS::VerificationStatus::Unknown},
    };

    // Remove previous badge widgets (tagged with "verificationBadge" object name)
    auto oldBadges = section->findChildren<QWidget*>(QStringLiteral("verificationBadge"));
    for (auto* w : oldBadges)
        delete w;

    auto* badgeContainer = new QWidget(section);
    badgeContainer->setObjectName(QStringLiteral("verificationBadge"));

    auto* row = new QHBoxLayout(badgeContainer);
    row->setContentsMargins(0, 2, 0, 0);
    row->setSpacing(12);

    for (const auto& r : results) {
        auto* item = new QHBoxLayout();
        item->setSpacing(3);

        auto* icon = new QLabel(badgeContainer);
        auto* text = new QLabel(r.label, badgeContainer);

        switch (r.result) {
        case LibreSCRS::VerificationStatus::Valid:
            icon->setText(QStringLiteral("\u2714"));
            icon->setStyleSheet("color: #4CAF50; font-size: 12px;");
            text->setStyleSheet("color: #4CAF50; font-size: 10px;");
            break;
        case LibreSCRS::VerificationStatus::Invalid:
            icon->setText(QStringLiteral("\u2718"));
            icon->setStyleSheet("color: #F44336; font-size: 12px;");
            text->setStyleSheet("color: #F44336; font-size: 10px;");
            break;
        case LibreSCRS::VerificationStatus::Unknown:
            icon->setText(QStringLiteral("?"));
            icon->setStyleSheet("color: #9E9E9E; font-size: 12px;");
            text->setStyleSheet("color: #9E9E9E; font-size: 10px;");
            break;
        }

        item->addWidget(icon);
        item->addWidget(text);
        row->addLayout(item);
    }

    row->addStretch();

    // Append badge container below the grid — FieldSectionBuilder uses QGridLayout
    if (auto* grid = qobject_cast<QGridLayout*>(section->layout()))
        grid->addWidget(badgeContainer, grid->rowCount(), 0, 1, 2);
}

CollapsibleSection* EidWidget::buildAddressSection(QWidget* parent) const
{
    const auto* addressGroup = data.findGroup("address");

    // Build address fields with composite assembly (same logic as original)
    auto* section = new CollapsibleSection(
        isForeigner() ? qtTrId("lc-eid-label-address-foreigner") : qtTrId("lc-eid-label-address"), parent);

    auto* grid = new QGridLayout();
    grid->setSpacing(4);

    auto addField = [&](int row, int col, const QString& label, const QString& value, int colSpan = 1) {
        if (value.isEmpty())
            return;
        auto* cellLayout = new QVBoxLayout();
        cellLayout->setContentsMargins(0, 0, 0, 0);
        cellLayout->setSpacing(0);
        auto* lbl = new QLabel(label, section);
        lbl->setStyleSheet("color: #777; font-size: 10px;");
        auto* val = new QLineEdit(value, section);
        val->setReadOnly(true);
        val->setCursorPosition(0);
        cellLayout->addWidget(lbl);
        cellLayout->addWidget(val);
        grid->addLayout(cellLayout, row, col, 1, colSpan);
    };

    // Assemble composite address
    QStringList addressParts;
    addressParts << getFieldValue(addressGroup, "place") << getFieldValue(addressGroup, "community")
                 << getFieldValue(addressGroup, "street") << getFieldValue(addressGroup, "house_number");
    addressParts.removeAll(QString());
    auto address = addressParts.join(", ");

    auto floor = getFieldValue(addressGroup, "floor");
    auto apartmentNumber = getFieldValue(addressGroup, "apartment_number");
    if (!floor.isEmpty())
        address += "/" + floor;
    if (!apartmentNumber.isEmpty())
        address += "/" + apartmentNumber;

    addField(0, 0, qtTrId("lc-eid-label-address"), address, 2);

    // Address date (only if meaningful)
    auto addressDate = getFieldValue(addressGroup, "address_date");
    if (!addressDate.isEmpty() && addressDate != "01.01.0001") {
        addField(1, 0, qtTrId("lc-eid-label-address-date"), addressDate, 2);
    }

    section->setLayout(grid);
    return section;
}

CollapsibleSection* EidWidget::buildDocumentSection(QWidget* parent) const
{
    const auto* docGroup = data.findGroup("document");

    const std::map<std::string, QString> translationMap = {
        {"document_type", qtTrId("lc-eid-label-document-type")},
        {"document_serial_number", qtTrId("lc-eid-label-document-serial-number")},
        {"issuing_authority", qtTrId("lc-eid-label-issuing-authority")},
        {"doc_reg_no", qtTrId("lc-eid-label-doc-reg-no")},
        {"issuing_date", qtTrId("lc-eid-label-issuing-date")},
        {"expiry_date", qtTrId("lc-eid-label-expiry-date")},
    };

    // Fields not relevant for display in the document section
    const std::set<std::string> hiddenFields = {
        "card_type",
    };

    if (!docGroup) {
        // Return empty section if no document group
        auto* section = new CollapsibleSection(qtTrId("lc-eid-label-document"), parent);
        return section;
    }

    auto* section = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-eid-label-document"), *docGroup, translationMap,
                                                          hiddenFields, parent);
    LibreSCRS::FieldSectionBuilder::highlightExpiredDates(section, *docGroup, {"expiry_date"});
    return section;
}
