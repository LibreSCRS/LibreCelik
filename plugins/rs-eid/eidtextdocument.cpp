// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "eidtextdocument.h"
#include <plugin/carddatautils.h>

using plugin::getFieldValue;

EIdTextDocument::EIdTextDocument(const plugin::CardData& cardData, QString documentPath, QString cssPath)
{
    // Foreigner detection — matches EidWidget::isForeigner()
    const auto* cardTypeField = cardData.findField("card_type");
    isForeigner = cardTypeField && cardTypeField->asString() == "ForeignerIF2020";

    if (documentPath.isEmpty())
        documentPath = isForeigner ? QStringLiteral(":/html/idcardIF2020.html") : QStringLiteral(":/html/idcard.html");
    if (cssPath.isEmpty())
        cssPath = QStringLiteral(":/html/idcard.css");

    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, cardData);

    setupDocument(data, cssPath);
}

void EIdTextDocument::translateDocumentData(QString& data) const
{
    data.replace("${title}", qtTrId("lc-eid-doc-title"));
    data.replace("${foreigner_id}", qtTrId("lc-eid-doc-foreigner-id"));
    data.replace("${printing_date}", qtTrId("lc-eid-doc-printing-date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));

    data.replace("${foreigner_data}", qtTrId("lc-eid-foreigner-data"));
    data.replace("${citizen_data}", qtTrId("lc-eid-citizen-data"));
    data.replace("${last_name}", qtTrId("lc-eid-doc-surname"));
    data.replace("${first_name}", qtTrId("lc-eid-doc-name"));
    data.replace("${parent_name}", qtTrId("lc-eid-doc-parent-name"));
    data.replace("${nationality}", qtTrId("lc-eid-doc-nationality"));
    data.replace("${date_of_birth}", qtTrId("lc-eid-doc-date-birth"));
    data.replace("${place_of_birth}", qtTrId("lc-eid-doc-place-birth"));
    data.replace("${status_of_foreigner}", qtTrId("lc-eid-doc-foreigner-status"));

    if (isForeigner)
        data.replace("${adress}", qtTrId("lc-eid-label-address-foreigner"));
    else
        data.replace("${adress}", qtTrId("lc-eid-label-address"));

    data.replace("${date_of_address_change}", qtTrId("lc-eid-doc-address-change-date"));
    data.replace("${jmbg}", qtTrId("lc-eid-label-jmbg"));
    data.replace("${gender}", qtTrId("lc-eid-doc-gender"));

    data.replace("${document_data}", qtTrId("lc-eid-doc-document-data"));
    data.replace("${document_issuer}", qtTrId("lc-eid-doc-issuer"));
    data.replace("${document_number}", qtTrId("lc-eid-doc-number"));
    data.replace("${issuance_date}", qtTrId("lc-eid-doc-issuance-date"));
    data.replace("${validity_date}", qtTrId("lc-eid-doc-valid-to"));
}

void EIdTextDocument::prepareDocumentData(QString& html, const plugin::CardData& cardData) const
{
    html.replace("${last_name_value}", getPreparedValue(getFieldValue(cardData, "surname")));
    html.replace("${first_name_value}", getPreparedValue(getFieldValue(cardData, "given_name")));
    html.replace("${parent_name_value}", getPreparedValue(getFieldValue(cardData, "parent_given_name")));
    html.replace("${nationality_value}", getPreparedValue(getFieldValue(cardData, "nationality")));
    html.replace("${date_of_birth_value}", getPreparedValue(getFieldValue(cardData, "date_of_birth")));

    // Composite: place of birth
    QStringList pobParts;
    pobParts << getFieldValue(cardData, "place_of_birth") << getFieldValue(cardData, "community_of_birth")
             << getFieldValue(cardData, "state_of_birth");
    pobParts.removeAll(QString());
    html.replace("${place_of_birth_value}", getPreparedValue(pobParts.join(", ")));

    html.replace("${status_of_foreigner_value}", getPreparedValue(getFieldValue(cardData, "status_of_foreigner")));

    // Composite: address (same assembly as EidWidget::buildAddressSection)
    QStringList addrParts;
    addrParts << getFieldValue(cardData, "state") << getFieldValue(cardData, "community")
              << getFieldValue(cardData, "place") << getFieldValue(cardData, "street")
              << getFieldValue(cardData, "house_number");
    auto houseLetter = getFieldValue(cardData, "house_letter");
    if (!houseLetter.isEmpty())
        addrParts << houseLetter;
    addrParts.removeAll(QString());
    auto address = addrParts.join(", ");
    auto entrance = getFieldValue(cardData, "entrance");
    auto floor = getFieldValue(cardData, "floor");
    auto apartment = getFieldValue(cardData, "apartment_number");
    if (!entrance.isEmpty())
        address += "/" + entrance;
    if (!floor.isEmpty())
        address += "/" + floor;
    if (!apartment.isEmpty())
        address += "/" + apartment;
    html.replace("${adress_value}", getPreparedValue(address));

    html.replace("${date_of_address_change_value}", getPreparedValue(getFieldValue(cardData, "address_date")));
    html.replace("${jmbg_value}", getPreparedValue(getFieldValue(cardData, "personal_number")));
    html.replace("${gender_value}", getPreparedValue(getFieldValue(cardData, "sex")));

    html.replace("${document_issuer_value}", getPreparedValue(getFieldValue(cardData, "issuing_authority")));
    html.replace("${document_number_value}", getPreparedValue(getFieldValue(cardData, "doc_reg_no")));
    html.replace("${issuance_date_value}", getPreparedValue(getFieldValue(cardData, "issuing_date")));
    html.replace("${validity_date_value}", getPreparedValue(getFieldValue(cardData, "expiry_date")));

    // Photo — raw bytes to base64 data URI
    const auto* photoField = cardData.findField("photo");
    if (photoField && !photoField->value.empty()) {
        QByteArray raw(reinterpret_cast<const char*>(photoField->value.data()),
                       static_cast<qsizetype>(photoField->value.size()));
        QString dataUri = "data:image/png;base64, " + raw.toBase64();
        html.replace(":/images/user.png", dataUri);
    }
}
