// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "eidwidget.h"

#include "utils/cardheadercard.h"
#include "utils/collapsiblesection.h"
#include "utils/fieldsectionbuilder.h"

#include <plugin/carddatautils.h>

#include <QDate>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

using plugin::getFieldValue;

EidWidget::EidWidget(const plugin::CardData& cardData, QWidget* parent) : QWidget(parent), data(cardData)
{
    buildLayout();
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

void EidWidget::buildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    auto* outerSection = new CollapsibleSection(
        isForeigner() ? qtTrId("lc-eid-title-foreigner") : qtTrId("lc-eid-title"), QColor(34, 86, 117), this);
    outerSection->setHeaderHeight(56);

    auto* sectionLayout = new QVBoxLayout();
    sectionLayout->setSpacing(6);

    // --- Header card: photo + key personal fields ---
    const auto* personal = data.findGroup("personal");
    bool foreigner = isForeigner();

    std::vector<LibreSCRS::HeaderField> headerFields;
    headerFields.push_back({qtTrId("lc-eid-label-given-name"), getFieldValue(personal, "given_name"), 1});
    headerFields.push_back({qtTrId("lc-eid-label-surname"), getFieldValue(personal, "surname"), 1});
    headerFields.push_back({qtTrId("lc-eid-label-date-of-birth"), getFieldValue(personal, "date_of_birth"), 1});
    headerFields.push_back({foreigner ? qtTrId("lc-eid-label-ebs") : qtTrId("lc-eid-label-jmbg"),
                            getFieldValue(personal, "personal_number"), 1});
    headerFields.push_back({qtTrId("lc-eid-label-sex"), getFieldValue(personal, "sex"), 1});

    if (foreigner) {
        headerFields.push_back({qtTrId("lc-eid-label-nationality"), getFieldValue(personal, "nationality"), 1});

        // Assemble place of birth from components
        QStringList pobParts;
        pobParts << getFieldValue(personal, "place_of_birth") << getFieldValue(personal, "community_of_birth")
                 << getFieldValue(personal, "state_of_birth");
        pobParts.removeAll(QString());
        headerFields.push_back({qtTrId("lc-eid-label-place-of-birth"), pobParts.join(", "), 2});
    } else {
        headerFields.push_back({qtTrId("lc-eid-label-parent-name"), getFieldValue(personal, "parent_given_name"), 1});
    }

    auto* headerCard = new LibreSCRS::CardHeaderCard(loadPhoto(), QSize(190, 250), headerFields, outerSection);

    // Verification indicators — read from middleware meta group
    const auto* meta = data.findGroup("meta");
    if (meta) {
        auto toResult = [](const QString& val) -> LibreSCRS::VerificationStatus::Result {
            if (val == "valid")
                return LibreSCRS::VerificationStatus::Valid;
            if (val == "invalid")
                return LibreSCRS::VerificationStatus::Invalid;
            return LibreSCRS::VerificationStatus::Unknown;
        };
        headerCard->setVerificationResults({
            {qtTrId("lc-eid-label-card-verification"), toResult(getFieldValue(meta, "card_verification"))},
            {qtTrId("lc-eid-label-fixed-verification"), toResult(getFieldValue(meta, "fixed_verification"))},
            {qtTrId("lc-eid-label-variable-verification"), toResult(getFieldValue(meta, "variable_verification"))},
        });
    }

    sectionLayout->addWidget(headerCard);

    // --- Inner sections: Address + Document (stacked vertically) ---
    auto* addressSection = buildAddressSection(outerSection);
    auto* documentSection = buildDocumentSection(outerSection);

    sectionLayout->addWidget(addressSection);
    sectionLayout->addWidget(documentSection);

    outerSection->setLayout(sectionLayout);
    outerSection->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);

    layout->addWidget(outerSection);
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

    return LibreSCRS::FieldSectionBuilder::build(qtTrId("lc-eid-label-document"), *docGroup, translationMap,
                                                 hiddenFields, parent);
}
