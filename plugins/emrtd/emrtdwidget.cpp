// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "emrtdwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"

#include <plugin/carddatautils.h>

#include <QLabel>
#include <QLineEdit>
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

    // CardHeaderCard — photo + key fields
    QPixmap photo;
    if (const auto* photoGroup = data.findGroup("photo")) {
        if (!photoGroup->fields.empty() && !photoGroup->fields[0].value.empty()) {
            photo.loadFromData(photoGroup->fields[0].value.data(),
                               static_cast<uint>(photoGroup->fields[0].value.size()));
        }
    }

    const auto* personalGroup = data.findGroup("personal");
    const auto* docGroup = data.findGroup("document");

    std::vector<LibreSCRS::HeaderField> headerFields;
    if (personalGroup) {
        headerFields.push_back({"Given Names", getFieldValue(personalGroup, "given_names")});
        headerFields.push_back({"Surname", getFieldValue(personalGroup, "surname")});
        headerFields.push_back({"Nationality", getFieldValue(personalGroup, "nationality")});
        headerFields.push_back({"Date of Birth", getFieldValue(personalGroup, "date_of_birth")});
    }
    if (docGroup) {
        headerFields.push_back({"Document No.", getFieldValue(docGroup, "document_number")});
        headerFields.push_back({"Expiry", getFieldValue(docGroup, "date_of_expiry")});
    }

    if (!photo.isNull()) {
        auto* headerCard = new LibreSCRS::CardHeaderCard(photo, QSize(190, 250), headerFields, travelDocSection);
        sectionLayout->addWidget(headerCard);
    } else if (!headerFields.empty()) {
        // No photo — show fields only with a placeholder
        QPixmap placeholder(190, 250);
        placeholder.fill(QColor(220, 220, 220));
        auto* headerCard =
            new LibreSCRS::CardHeaderCard(placeholder, QSize(190, 250), headerFields, travelDocSection);
        sectionLayout->addWidget(headerCard);
    }

    // Document Data, Additional — stacked vertically
    if (docGroup) {
        auto* docSection = LibreSCRS::FieldSectionBuilder::build("Document Data", *docGroup, documentTranslationMap);
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
