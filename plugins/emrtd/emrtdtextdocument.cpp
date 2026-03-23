// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <QCoreApplication>
#include <QDate>
#include "emrtdtextdocument.h"
#include <plugin/carddatautils.h>

using plugin::getFieldValue;

EMRTDTextDocument::EMRTDTextDocument(const plugin::CardData& cardData, QString documentPath, QString cssPath)
{
    if (documentPath.isEmpty())
        documentPath = QStringLiteral(":/html/emrtdcard.html");
    if (cssPath.isEmpty())
        cssPath = QStringLiteral(":/html/emrtdcard.css");

    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, cardData);

    setupDocument(data, cssPath);
}

void EMRTDTextDocument::translateDocumentData(QString& data) const
{
    data.replace("${title}", qtTrId("lc-emrtd-doc-title"));
    data.replace("${printing_date}", qtTrId("lc-emrtd-doc-printing-date"));
    data.replace("${printing_date_value}", QDate::currentDate().toString("dd.MM.yyyy"));

    // Personal section — reuse existing widget translation IDs where available
    data.replace("${personal_data}", qtTrId("lc-personal-data-title"));
    data.replace("${surname}", qtTrId("lc-emrtd-surname"));
    data.replace("${given_names}", qtTrId("lc-emrtd-given-names"));
    data.replace("${nationality}", qtTrId("lc-emrtd-nationality"));
    data.replace("${date_of_birth}", qtTrId("lc-emrtd-date-of-birth"));
    data.replace("${sex}", qtTrId("lc-emrtd-doc-sex"));

    // Document section
    data.replace("${document_data}", qtTrId("lc-emrtd-document-data"));
    data.replace("${document_number}", qtTrId("lc-emrtd-doc-number"));
    data.replace("${document_code}", qtTrId("lc-emrtd-doc-code"));
    data.replace("${issuing_state}", qtTrId("lc-emrtd-issuing-state"));
    data.replace("${date_of_expiry}", qtTrId("lc-emrtd-date-of-expiry"));
    data.replace("${personal_number}", qtTrId("lc-emrtd-personal-number"));

    // DG11 section
    data.replace("${additional_data}", qtTrId("lc-emrtd-additional"));
    data.replace("${full_name}", qtTrId("lc-emrtd-full-name"));
    data.replace("${place_of_birth}", qtTrId("lc-emrtd-place-of-birth"));
    data.replace("${address}", qtTrId("lc-emrtd-address"));
    data.replace("${telephone}", qtTrId("lc-emrtd-telephone"));
    data.replace("${profession}", qtTrId("lc-emrtd-profession"));

    // DG12 section
    data.replace("${issuing_info}", qtTrId("lc-emrtd-issuing-info"));
    data.replace("${issuing_authority}", qtTrId("lc-emrtd-issuing-authority"));
    data.replace("${date_of_issue}", qtTrId("lc-emrtd-date-of-issue"));
    data.replace("${endorsements}", qtTrId("lc-emrtd-endorsements"));

    // DG7 section
    data.replace("${signature_label}", qtTrId("lc-emrtd-signature"));
}

void EMRTDTextDocument::prepareDocumentData(QString& html, const plugin::CardData& cardData) const
{
    // Personal fields
    html.replace("${surname_value}", getPreparedValue(getFieldValue(cardData, "surname")));
    html.replace("${given_names_value}", getPreparedValue(getFieldValue(cardData, "given_names")));
    html.replace("${nationality_value}", getPreparedValue(getFieldValue(cardData, "nationality")));
    html.replace("${date_of_birth_value}", getPreparedValue(getFieldValue(cardData, "date_of_birth")));
    html.replace("${sex_value}", getPreparedValue(getFieldValue(cardData, "sex")));

    // Document fields
    html.replace("${document_number_value}", getPreparedValue(getFieldValue(cardData, "document_number")));
    html.replace("${document_code_value}", getPreparedValue(getFieldValue(cardData, "document_code")));
    html.replace("${issuing_state_value}", getPreparedValue(getFieldValue(cardData, "issuing_state")));
    html.replace("${date_of_expiry_value}", getPreparedValue(getFieldValue(cardData, "date_of_expiry")));
    html.replace("${personal_number_value}", getPreparedValue(getFieldValue(cardData, "personal_number")));

    // Photo — raw bytes to base64 data URI
    const auto* photoField = cardData.findField("photo");
    if (photoField && !photoField->value.empty()) {
        QByteArray raw(reinterpret_cast<const char*>(photoField->value.data()),
                       static_cast<qsizetype>(photoField->value.size()));
        QString dataUri = "data:image/png;base64, " + raw.toBase64();
        html.replace(":/images/user.png", dataUri);
    }

    // DG11 — Additional Personal Data (optional)
    const auto* dg11 = cardData.findGroup("additional");
    if (dg11) {
        html.replace("${full_name_value}", getPreparedValue(getFieldValue(dg11, "full_name")));
        html.replace("${place_of_birth_value}", getPreparedValue(getFieldValue(dg11, "place_of_birth")));
        html.replace("${address_value}", getPreparedValue(getFieldValue(dg11, "address")));
        html.replace("${telephone_value}", getPreparedValue(getFieldValue(dg11, "telephone")));
        html.replace("${profession_value}", getPreparedValue(getFieldValue(dg11, "profession")));
    } else {
        removeConditionalBlock(html, "DG11");
    }

    // DG12 — Issuing Information (optional)
    const auto* dg12 = cardData.findGroup("document_extra");
    if (dg12) {
        html.replace("${issuing_authority_value}", getPreparedValue(getFieldValue(dg12, "issuing_authority")));
        html.replace("${date_of_issue_value}", getPreparedValue(getFieldValue(dg12, "date_of_issue")));
        html.replace("${endorsements_value}", getPreparedValue(getFieldValue(dg12, "endorsements")));
    } else {
        removeConditionalBlock(html, "DG12");
    }

    // DG7 — Signature (optional)
    const auto* sigGroup = cardData.findGroup("signature");
    if (sigGroup && !sigGroup->fields.empty() && !sigGroup->fields[0].value.empty()) {
        QByteArray sigRaw(reinterpret_cast<const char*>(sigGroup->fields[0].value.data()),
                          static_cast<qsizetype>(sigGroup->fields[0].value.size()));
        QString sigUri = "data:image/png;base64, " + sigRaw.toBase64();
        html.replace("${signature_image}", sigUri);
    } else {
        removeConditionalBlock(html, "DG7");
    }
}

void EMRTDTextDocument::removeConditionalBlock(QString& html, const QString& marker) const
{
    QString startMarker = "<!-- " + marker + "_START -->";
    QString endMarker = "<!-- " + marker + "_END -->";
    int startIdx = html.indexOf(startMarker);
    int endIdx = html.indexOf(endMarker);
    if (startIdx >= 0 && endIdx >= 0) {
        html.remove(startIdx, endIdx + endMarker.length() - startIdx);
    }
}
