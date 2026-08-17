// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "eidwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"
#include "utils/iconutils.h"

#include <plugin/fieldvalue.h>

#include <QDate>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QToolButton>
#include <QVBoxLayout>

using librecelik::plugin::fieldDetailBytes;
using librecelik::plugin::fieldValue;
using librecelik::plugin::findGroup;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

/// The portrait the gateway merged into the read: its own group, keyed with
/// the field half of the wire's composite key. A group that carries the image
/// under some other key still renders — the first field with bytes wins.
QByteArray photoBytes(const QList<FieldGroup>& groups)
{
    QByteArray bytes = fieldDetailBytes(groups, u"photo", u"photo");
    if (!bytes.isEmpty()) {
        return bytes;
    }
    if (const FieldGroup* group = findGroup(groups, u"photo")) {
        for (const Field& field : group->fields) {
            bytes = field.detail.toByteArray();
            if (!bytes.isEmpty()) {
                return bytes;
            }
        }
    }
    return {};
}

} // namespace

EidWidget::EidWidget(const QList<FieldGroup>& cardGroups, QWidget* parent) : EidWidget(parent)
{
    // Staged: addGroup's sections chain ("meta" raises the shell, "personal"
    // hangs the photo row and the badge host off it) and the final wire
    // model's order is delivery-dependent (see stagedForBuild).
    for (const auto& group : librecelik::plugin::stagedForBuild(
             cardGroups, {u"meta", u"personal", u"address", u"document", u"photo", u"verification"}))
        addGroup(group);
    applyVerificationFromMeta();
}

void EidWidget::applyVerificationFromMeta()
{
    // Non-streaming path: verification data may be in "meta" group rather
    // than a separate "verification" group. Apply if streaming didn't
    // handle it. Extracted from the full-data ctor so retranslateUi()
    // can re-trigger the badge step after rebuilding sections.
    if (personalSection && !findGroup(groups, u"verification")) {
        if (const FieldGroup* meta = findGroup(groups, u"meta"))
            addVerificationBadges(personalSection, meta);
    }
}

EidWidget::EidWidget(QWidget* parent) : plugin_ui::PluginWidgetBase(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
}

void EidWidget::addGroup(const FieldGroup& group)
{
    groups.append(group);

    const QString key = group.key;

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
        printBtn = iconutils::createPrinterHeaderButton(this);
        connect(printBtn, &QToolButton::clicked, this, [this]() { emit printRequested(groups); });
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
    return fieldValue(groups, u"card_type") == QLatin1String("ForeignerIF2020");
}

QPixmap EidWidget::loadPhoto() const
{
    const QByteArray bytes = photoBytes(groups);
    if (!bytes.isEmpty()) {
        QPixmap pixmap;
        if (pixmap.loadFromData(bytes))
            return pixmap;
    }
    return QPixmap(QStringLiteral(":/images/user.png"));
}

CollapsibleSection* EidWidget::buildPersonalSection(QWidget* parent) const
{
    const FieldGroup* personal = findGroup(groups, u"personal");
    if (!personal) {
        auto* section = new CollapsibleSection(qtTrId("lc-personal-data-title"), parent);
        section->setCollapsible(false);
        return section;
    }

    bool foreigner = isForeigner();

    std::map<QString, QString> translationMap = {
        {QStringLiteral("given_name"), qtTrId("lc-eid-label-given-name")},
        {QStringLiteral("surname"), qtTrId("lc-eid-label-surname")},
        {QStringLiteral("date_of_birth"), qtTrId("lc-eid-label-date-of-birth")},
        {QStringLiteral("personal_number"), foreigner ? qtTrId("lc-eid-label-ebs") : qtTrId("lc-eid-label-jmbg")},
        {QStringLiteral("sex"), qtTrId("lc-eid-label-sex")},
    };

    std::set<QString> hiddenFields = {
        QStringLiteral("place_of_birth"),
        QStringLiteral("community_of_birth"),
        QStringLiteral("state_of_birth"),
    };

    if (foreigner) {
        translationMap[QStringLiteral("nationality")] = qtTrId("lc-eid-label-nationality");
        translationMap[QStringLiteral("status_of_foreigner")] = qtTrId("lc-eid-label-status-of-foreigner");
        hiddenFields.insert(QStringLiteral("parent_given_name"));
    } else {
        translationMap[QStringLiteral("parent_given_name")] = qtTrId("lc-eid-label-parent-name");
        hiddenFields.insert(QStringLiteral("nationality"));
        hiddenFields.insert(QStringLiteral("status_of_foreigner"));
    }

    // Synthetic composite place-of-birth for foreigners
    FieldGroup modifiedGroup = *personal;
    if (foreigner) {
        QStringList pobParts;
        pobParts << fieldValue(*personal, u"place_of_birth") << fieldValue(*personal, u"community_of_birth")
                 << fieldValue(*personal, u"state_of_birth");
        pobParts.removeAll(QString());
        auto pobValue = pobParts.join(", ");
        if (!pobValue.isEmpty()) {
            Field composite;
            composite.key = QStringLiteral("place_of_birth_composite");
            composite.value = pobValue;
            composite.extra.insert(QStringLiteral("labelFallback"), QStringLiteral("Place of Birth"));
            composite.extra.insert(QStringLiteral("type"), QStringLiteral("text"));
            modifiedGroup.fields.append(composite);
            translationMap[QStringLiteral("place_of_birth_composite")] = qtTrId("lc-eid-label-place-of-birth");
        }
    }

    auto* section = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-personal-data-title"), modifiedGroup,
                                                                  translationMap, hiddenFields, parent);
    section->setCollapsible(false);
    return section;
}

void EidWidget::addVerificationBadges(CollapsibleSection* section, const FieldGroup* source)
{
    if (!section)
        return;

    auto toResult = [](const QString& val) -> librecelik::utils::VerificationStatus::Result {
        if (val == "valid")
            return librecelik::utils::VerificationStatus::Valid;
        if (val == "invalid")
            return librecelik::utils::VerificationStatus::Invalid;
        return librecelik::utils::VerificationStatus::Unknown;
    };

    std::vector<librecelik::utils::VerificationStatus> results = {
        {qtTrId("lc-eid-label-card-verification"),
         source ? toResult(fieldValue(*source, u"card_verification")) : librecelik::utils::VerificationStatus::Unknown},
        {qtTrId("lc-eid-label-fixed-verification"), source ? toResult(fieldValue(*source, u"fixed_verification"))
                                                           : librecelik::utils::VerificationStatus::Unknown},
        {qtTrId("lc-eid-label-variable-verification"), source ? toResult(fieldValue(*source, u"variable_verification"))
                                                              : librecelik::utils::VerificationStatus::Unknown},
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
        case librecelik::utils::VerificationStatus::Valid:
            icon->setText(QStringLiteral("\u2714"));
            icon->setStyleSheet("color: #4CAF50; font-size: 12px;");
            text->setStyleSheet("color: #4CAF50; font-size: 10px;");
            break;
        case librecelik::utils::VerificationStatus::Invalid:
            icon->setText(QStringLiteral("\u2718"));
            icon->setStyleSheet("color: #F44336; font-size: 12px;");
            text->setStyleSheet("color: #F44336; font-size: 10px;");
            break;
        case librecelik::utils::VerificationStatus::Unknown:
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
    addressParts << fieldValue(groups, u"address", u"place") << fieldValue(groups, u"address", u"community")
                 << fieldValue(groups, u"address", u"street") << fieldValue(groups, u"address", u"house_number");
    addressParts.removeAll(QString());
    auto address = addressParts.join(", ");

    auto floor = fieldValue(groups, u"address", u"floor");
    auto apartmentNumber = fieldValue(groups, u"address", u"apartment_number");
    if (!floor.isEmpty())
        address += "/" + floor;
    if (!apartmentNumber.isEmpty())
        address += "/" + apartmentNumber;

    addField(0, 0, qtTrId("lc-eid-label-address"), address, 2);

    // Address date (only if meaningful)
    auto addressDate = fieldValue(groups, u"address", u"address_date");
    if (!addressDate.isEmpty() && addressDate != "01.01.0001") {
        addField(1, 0, qtTrId("lc-eid-label-address-date"), addressDate, 2);
    }

    section->setLayout(grid);
    return section;
}

CollapsibleSection* EidWidget::buildDocumentSection(QWidget* parent) const
{
    const FieldGroup* docGroup = findGroup(groups, u"document");

    const std::map<QString, QString> translationMap = {
        {QStringLiteral("document_type"), qtTrId("lc-eid-label-document-type")},
        {QStringLiteral("document_serial_number"), qtTrId("lc-eid-label-document-serial-number")},
        {QStringLiteral("issuing_authority"), qtTrId("lc-eid-label-issuing-authority")},
        {QStringLiteral("doc_reg_no"), qtTrId("lc-eid-label-doc-reg-no")},
        {QStringLiteral("issuing_date"), qtTrId("lc-eid-label-issuing-date")},
        {QStringLiteral("expiry_date"), qtTrId("lc-eid-label-expiry-date")},
    };

    // Fields not relevant for display in the document section
    const std::set<QString> hiddenFields = {
        QStringLiteral("card_type"),
    };

    if (!docGroup) {
        // Return empty section if no document group
        auto* section = new CollapsibleSection(qtTrId("lc-eid-label-document"), parent);
        return section;
    }

    auto* section = librecelik::utils::FieldSectionBuilder::build(qtTrId("lc-eid-label-document"), *docGroup,
                                                                  translationMap, hiddenFields, parent);
    librecelik::utils::FieldSectionBuilder::highlightExpiredDates(section, *docGroup, {QStringLiteral("expiry_date")});
    return section;
}

void EidWidget::retranslateUi()
{
    // Plugin widget rebuild-tier (April 2026 retranslate spec): tear
    // down all dynamic sections and rebuild from the cached groups so
    // the new translator is applied to every label, header, and badge.
    if (groups.isEmpty()) {
        // Nothing built yet (empty-shell ctor before any addGroup()).
        return;
    }

    auto cachedGroups = std::move(groups);
    groups.clear();

    if (outerSection) {
        outerLayout->removeWidget(outerSection);
        outerSection->deleteLater();
        outerSection = nullptr;
    }
    sectionLayout = nullptr;
    photoLabel = nullptr;
    personalSection = nullptr;
    printBtn = nullptr;

    for (const auto& group : cachedGroups)
        addGroup(group);
    applyVerificationFromMeta();
}
