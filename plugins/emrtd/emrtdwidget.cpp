// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidget.h"

#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"

#include <QHBoxLayout>

#include <plugin/carddatautils.h>

#include <QLabel>
#include <QLineEdit>
#include <QPainter>
#include <QPixmap>
#include <QVBoxLayout>

using plugin::getFieldValue;

namespace {

// Translation maps — readable English fallbacks for now (Task 10 will add qtTrId calls)
const std::map<std::string, QString> documentTranslationMap = {
    {"document_number", "Document Number"}, {"document_code", "Document Code"},     {"issuing_state", "Issuing State"},
    {"date_of_expiry", "Date of Expiry"},   {"personal_number", "Personal Number"},
};

const std::map<std::string, QString> documentExtraTranslationMap = {
    {"issuing_authority", "Issuing Authority"},
    {"date_of_issue", "Date of Issue"},
    {"endorsements", "Endorsements"},
    {"tax_exit", "Tax/Exit Requirements"},
};

const std::map<std::string, QString> personalTranslationMap = {
    {"given_names", qtTrId("lc-emrtd-given-names")},
    {"surname", qtTrId("lc-emrtd-surname")},
    {"nationality", qtTrId("lc-emrtd-nationality")},
    {"date_of_birth", qtTrId("lc-emrtd-date-of-birth")},
};

const std::map<std::string, QString> additionalTranslationMap = {
    {"full_name", "Full Name"},
    {"other_names", "Other Names"},
    {"personal_number", "Personal Number"},
    {"place_of_birth", "Place of Birth"},
    {"address", "Address"},
    {"telephone", "Telephone"},
    {"profession", "Profession"},
    {"title", "Title"},
    {"custody_info", "Custody Information"},
};

} // namespace

EMRTDWidget::EMRTDWidget(const plugin::CardData& cardData, QWidget* parent) : QWidget(parent), data(cardData)
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);

    // Phase 1: auth required — show status
    if (const auto* authGroup = data.findGroup("auth_required")) {
        showAuthRequired(authGroup);
        return;
    }

    // Error state
    if (const auto* errorGroup = data.findGroup("error")) {
        showError(errorGroup);
        return;
    }

    // Phase 2: authenticated — show passport data
    showPersonalData(data);
}

EMRTDWidget::EMRTDWidget(QWidget* parent) : QWidget(parent)
{
    outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);

    // Navy outer CollapsibleSection — shell for Phase 2 travel document display
    static const QColor navy(34, 86, 117);
    outerSection = new CollapsibleSection("Travel Document", navy, this);
    outerSection->setHeaderHeight(56);

    sectionLayout = new QVBoxLayout();
    sectionLayout->setSpacing(6);

    outerSection->setLayout(sectionLayout);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    outerLayout->addWidget(outerSection);
}

void EMRTDWidget::addGroup(const plugin::CardFieldGroup& group)
{
    const auto& key = group.groupKey;

    // Phase 1 groups are handled by createWidget — ignore here
    if (key == "auth_required" || key == "error")
        return;

    // Store the group for cardData() access
    data.groups.push_back(group);

    if (key == "personal") {
        auto* photoRow = new QHBoxLayout();
        photoRow->setSpacing(10);

        photoLabel = new QLabel(outerSection);
        QPixmap placeholder(190, 250);
        placeholder.fill(QColor(220, 220, 220));
        {
            QPixmap userIcon(QStringLiteral(":/images/user.png"));
            auto scaled = userIcon.scaled(QSize(95, 125), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPainter painter(&placeholder);
            painter.drawPixmap((190 - scaled.width()) / 2, (250 - scaled.height()) / 2, scaled);
        }
        photoLabel->setPixmap(placeholder);
        photoLabel->setFixedSize(190, 250);
        photoRow->addWidget(photoLabel, 0, Qt::AlignTop);

        auto* personalSec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-personal-data-title"), group,
                                                                  personalTranslationMap, {}, outerSection);
        personalSec->setCollapsible(false);
        photoRow->addWidget(personalSec, 1);

        sectionLayout->insertLayout(0, photoRow);
    } else if (key == "document") {
        auto* docSection = LibreSCRS::FieldSectionBuilder::build("Document Data", group, documentTranslationMap);
        LibreSCRS::FieldSectionBuilder::highlightExpiredDates(docSection, group, {"date_of_expiry"});
        sectionLayout->addWidget(docSection);
    } else if (key == "photo") {
        if (!photoLabel || group.fields.empty() || group.fields[0].value.empty())
            return;
        QPixmap photo;
        photo.loadFromData(group.fields[0].value.data(), static_cast<uint>(group.fields[0].value.size()));
        if (!photo.isNull()) {
            auto scaledPhoto = photo.scaledToHeight(250, Qt::SmoothTransformation);
            photoLabel->setFixedSize(scaledPhoto.size());
            photoLabel->setPixmap(scaledPhoto);
        }
    } else if (key == "signature") {
        if (!group.fields.empty() && !group.fields[0].value.empty()) {
            auto* sigSection = new CollapsibleSection("Signature / Mark", outerSection);
            auto* sigLayout = new QVBoxLayout();
            auto* sigLabel = new QLabel();
            QPixmap sigPixmap;
            sigPixmap.loadFromData(group.fields[0].value.data(), static_cast<uint>(group.fields[0].value.size()));
            sigLabel->setPixmap(sigPixmap.scaled(200, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sigLabel->setAlignment(Qt::AlignCenter);
            sigLayout->addWidget(sigLabel);
            sigSection->setLayout(sigLayout);
            sectionLayout->addWidget(sigSection);
        }
    } else if (key == "additional") {
        auto* additionalSection = LibreSCRS::FieldSectionBuilder::build("Additional", group, additionalTranslationMap);
        sectionLayout->addWidget(additionalSection);
    } else if (key == "document_extra") {
        // If "additional" already added, show as separate "Issuing Information" section
        // Otherwise show as "Additional"
        bool hasAdditional = data.findGroup("additional") != nullptr;
        if (hasAdditional) {
            auto* extraSection =
                LibreSCRS::FieldSectionBuilder::build("Issuing Information", group, documentExtraTranslationMap);
            sectionLayout->addWidget(extraSection);
        } else {
            auto* extraSection =
                LibreSCRS::FieldSectionBuilder::build("Additional", group, documentExtraTranslationMap);
            sectionLayout->addWidget(extraSection);
        }
    }
}

void EMRTDWidget::showAuthRequired(const plugin::CardFieldGroup* group)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* titleLabel = new QLabel(qtTrId("lc-emrtd-auth-required"));
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #E6873C;");
    layout->addWidget(titleLabel);

    auto* statusLabel = new QLabel(getFieldValue(group, "status"));
    layout->addWidget(statusLabel);

    auto paceSupported = getFieldValue(group, "pace_supported");
    if (paceSupported == "true") {
        auto* paceLabel = new QLabel("PACE: " + getFieldValue(group, "pace_oids"));
        paceLabel->setWordWrap(true);
        layout->addWidget(paceLabel);
    }

    auto* infoLabel = new QLabel(qtTrId("lc-emrtd-insert-mrz-hint"));
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    layout->addStretch();
}

void EMRTDWidget::showPersonalData(const plugin::CardData& data)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    // Navy outer CollapsibleSection
    static const QColor navy(34, 86, 117);
    auto* travelDocSection = new CollapsibleSection("Travel Document", navy, this);
    travelDocSection->setHeaderHeight(56);
    auto* sectionLayout = new QVBoxLayout();
    sectionLayout->setSpacing(6);

    // Photo + Personal section in HBoxLayout
    auto* photoRow = new QHBoxLayout();
    photoRow->setSpacing(10);

    QPixmap photo;
    if (const auto* photoGroup = data.findGroup("photo")) {
        if (!photoGroup->fields.empty() && !photoGroup->fields[0].value.empty()) {
            photo.loadFromData(photoGroup->fields[0].value.data(),
                               static_cast<uint>(photoGroup->fields[0].value.size()));
        }
    }

    auto* photoLbl = new QLabel(travelDocSection);
    if (photo.isNull()) {
        QPixmap placeholder(190, 250);
        placeholder.fill(QColor(220, 220, 220));
        {
            QPixmap userIcon(QStringLiteral(":/images/user.png"));
            auto scaled = userIcon.scaled(QSize(95, 125), Qt::KeepAspectRatio, Qt::SmoothTransformation);
            QPainter painter(&placeholder);
            painter.drawPixmap((190 - scaled.width()) / 2, (250 - scaled.height()) / 2, scaled);
        }
        photoLbl->setPixmap(placeholder);
        photoLbl->setFixedSize(190, 250);
    } else {
        auto scaledPhoto = photo.scaledToHeight(250, Qt::SmoothTransformation);
        photoLbl->setFixedSize(scaledPhoto.size());
        photoLbl->setPixmap(scaledPhoto);
    }
    photoRow->addWidget(photoLbl, 0, Qt::AlignTop);

    const auto* personalGroup = data.findGroup("personal");
    if (personalGroup) {
        auto* personalSec = LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-personal-data-title"), *personalGroup,
                                                                  personalTranslationMap, {}, travelDocSection);
        personalSec->setCollapsible(false);
        photoRow->addWidget(personalSec, 1);
    }

    sectionLayout->addLayout(photoRow);

    // Document Data, Additional — stacked vertically
    const auto* docGroup = data.findGroup("document");
    if (docGroup) {
        auto* docSection = LibreSCRS::FieldSectionBuilder::build("Document Data", *docGroup, documentTranslationMap);
        LibreSCRS::FieldSectionBuilder::highlightExpiredDates(docSection, *docGroup, {"date_of_expiry"});
        sectionLayout->addWidget(docSection);
    }

    // Merge document_extra into a section, or show additional
    const auto* docExtraGroup = data.findGroup("document_extra");
    const auto* additionalGroup = data.findGroup("additional");

    if (additionalGroup) {
        auto* additionalSection =
            LibreSCRS::FieldSectionBuilder::build("Additional", *additionalGroup, additionalTranslationMap);
        sectionLayout->addWidget(additionalSection);
    } else if (docExtraGroup) {
        auto* extraSection =
            LibreSCRS::FieldSectionBuilder::build("Additional", *docExtraGroup, documentExtraTranslationMap);
        sectionLayout->addWidget(extraSection);
    }

    // Document extra as separate section if both additional and document_extra exist
    if (docExtraGroup && additionalGroup) {
        auto* extraSection =
            LibreSCRS::FieldSectionBuilder::build("Issuing Information", *docExtraGroup, documentExtraTranslationMap);
        sectionLayout->addWidget(extraSection);
    }

    // Signature image (DG7) — teal CollapsibleSection, full width
    if (const auto* sigGroup = data.findGroup("signature")) {
        if (!sigGroup->fields.empty() && !sigGroup->fields[0].value.empty()) {
            auto* sigSection = new CollapsibleSection("Signature / Mark", travelDocSection);
            auto* sigLayout = new QVBoxLayout();
            auto* sigLabel = new QLabel();
            QPixmap sigPixmap;
            sigPixmap.loadFromData(sigGroup->fields[0].value.data(),
                                   static_cast<uint>(sigGroup->fields[0].value.size()));
            sigLabel->setPixmap(sigPixmap.scaled(200, 80, Qt::KeepAspectRatio, Qt::SmoothTransformation));
            sigLabel->setAlignment(Qt::AlignCenter);
            sigLayout->addWidget(sigLabel);
            sigSection->setLayout(sigLayout);
            sectionLayout->addWidget(sigSection);
        }
    }

    travelDocSection->setLayout(sectionLayout);
    travelDocSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
    layout->addWidget(travelDocSection);
}

void EMRTDWidget::showError(const plugin::CardFieldGroup* group)
{
    auto* layout = qobject_cast<QVBoxLayout*>(this->layout());

    auto* errorLabel = new QLabel(qtTrId("lc-emrtd-auth-failed"));
    errorLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #E6873C;");
    layout->addWidget(errorLabel);

    auto* detailLabel = new QLabel(getFieldValue(group, "error"));
    detailLabel->setWordWrap(true);
    layout->addWidget(detailLabel);

    layout->addStretch();
}
