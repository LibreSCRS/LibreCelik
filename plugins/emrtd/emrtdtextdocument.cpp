// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "emrtdtextdocument.h"
#include <plugin/carddatautils.h>
#include <LibreSCRS/Plugin/SecurityCheck.h>

using librecelik::plugin::getFieldValue;

EMRTDTextDocument::EMRTDTextDocument(const LibreSCRS::Plugin::CardData& cardData, QString documentPath, QString cssPath)
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
    data.replace(
        "${title}",
        qtTrId("lc-emrtd-doc-title")); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
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

    // Security status header
    data.replace("${security_integrity_label}", qtTrId("lc-emrtd-security-integrity"));
    data.replace("${security_authenticity_label}", qtTrId("lc-emrtd-security-authenticity"));
    data.replace("${security_genuineness_label}", qtTrId("lc-emrtd-security-genuineness"));

    // DG5 section
    data.replace("${portrait_label}", qtTrId("lc-emrtd-portrait"));

    // DG16 section
    data.replace("${contacts_label}", qtTrId("lc-emrtd-contacts"));
    data.replace("${contact_name}", qtTrId("lc-emrtd-contact-name"));
    data.replace("${contact_telephone}", qtTrId("lc-emrtd-telephone"));
    data.replace("${contact_address}", qtTrId("lc-emrtd-address"));
}

void EMRTDTextDocument::prepareDocumentData(QString& html, const LibreSCRS::Plugin::CardData& cardData) const
{
    // Security status — parse from security_status group
    if (auto secIdx = cardData.findGroup("security_status")) {
        const auto& secGroup = cardData.groupAt(*secIdx);
        auto statusColor = [](const QString& statusStr) -> QString {
            if (statusStr == "PASSED")
                return QStringLiteral("#4CAF50");
            if (statusStr == "FAILED")
                return QStringLiteral("#F44336");
            if (statusStr == "NOT_SUPPORTED" || statusStr == "SKIPPED")
                return QStringLiteral("#FFC107");
            return QStringLiteral("#9E9E9E");
        };
        auto statusLabel = [](const QString& statusStr) -> QString {
            if (statusStr == "PASSED")
                return qtTrId("lc-emrtd-security-passed");
            if (statusStr == "FAILED")
                return qtTrId("lc-emrtd-security-failed");
            if (statusStr == "NOT_SUPPORTED")
                return qtTrId("lc-emrtd-security-not-supported");
            if (statusStr == "SKIPPED")
                return qtTrId("lc-emrtd-security-skipped");
            return qtTrId("lc-emrtd-security-not-performed");
        };

        auto integrityStr = getFieldValue(&secGroup, "overall_integrity");
        auto authenticityStr = getFieldValue(&secGroup, "overall_authenticity");
        auto genuinenessStr = getFieldValue(&secGroup, "overall_genuineness");

        html.replace("${security_integrity_color}", statusColor(integrityStr));
        html.replace("${security_integrity_value}", statusLabel(integrityStr));
        html.replace("${security_authenticity_color}", statusColor(authenticityStr));
        html.replace("${security_authenticity_value}", statusLabel(authenticityStr));
        html.replace("${security_genuineness_color}", statusColor(genuinenessStr));
        html.replace("${security_genuineness_value}", statusLabel(genuinenessStr));
    } else {
        removeConditionalBlock(html, "SECURITY");
    }

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
    if (auto photoIdx = cardData.findField("photo")) {
        const auto& value = cardData.fieldAt(*photoIdx).value;
        if (!value.empty()) {
            QByteArray raw(reinterpret_cast<const char*>(value.data()), static_cast<qsizetype>(value.size()));
            QString dataUri = "data:image/png;base64, " + raw.toBase64();
            html.replace(":/images/user.png", dataUri);
        }
    }

    // DG11 — Additional Personal Data (optional)
    if (auto dg11Idx = cardData.findGroup("additional")) {
        const auto& dg11 = cardData.groupAt(*dg11Idx);
        html.replace("${full_name_value}", getPreparedValue(getFieldValue(&dg11, "full_name")));
        html.replace("${place_of_birth_value}", getPreparedValue(getFieldValue(&dg11, "place_of_birth")));
        html.replace("${address_value}", getPreparedValue(getFieldValue(&dg11, "address")));
        html.replace("${telephone_value}", getPreparedValue(getFieldValue(&dg11, "telephone")));
        html.replace("${profession_value}", getPreparedValue(getFieldValue(&dg11, "profession")));
    } else {
        removeConditionalBlock(html, "DG11");
    }

    // DG12 — Issuing Information (optional)
    if (auto dg12Idx = cardData.findGroup("document_extra")) {
        const auto& dg12 = cardData.groupAt(*dg12Idx);
        html.replace("${issuing_authority_value}", getPreparedValue(getFieldValue(&dg12, "issuing_authority")));
        html.replace("${date_of_issue_value}", getPreparedValue(getFieldValue(&dg12, "date_of_issue")));
        html.replace("${endorsements_value}", getPreparedValue(getFieldValue(&dg12, "endorsements")));
    } else {
        removeConditionalBlock(html, "DG12");
    }

    // DG5 — Portrait (optional)
    bool hasPortrait = false;
    if (auto portraitIdx = cardData.findGroup("portrait")) {
        const auto& portraitGroup = cardData.groupAt(*portraitIdx);
        if (!portraitGroup.fields.empty() && !portraitGroup.fields[0].value.empty()) {
            const auto& value = portraitGroup.fields[0].value;
            QByteArray portraitRaw(reinterpret_cast<const char*>(value.data()), static_cast<qsizetype>(value.size()));
            QString portraitUri = "data:image/png;base64, " + portraitRaw.toBase64();
            html.replace("${portrait_image}", portraitUri);
            hasPortrait = true;
        }
    }
    if (!hasPortrait) {
        removeConditionalBlock(html, "DG5");
    }

    // DG16 — Contacts (optional)
    if (auto contactsIdx = cardData.findGroup("contacts")) {
        const auto& contactsGroup = cardData.groupAt(*contactsIdx);
        html.replace("${contact_name_value}", getPreparedValue(getFieldValue(&contactsGroup, "name")));
        html.replace("${contact_telephone_value}", getPreparedValue(getFieldValue(&contactsGroup, "telephone")));
        html.replace("${contact_address_value}", getPreparedValue(getFieldValue(&contactsGroup, "address")));
    } else {
        removeConditionalBlock(html, "DG16");
    }

    // DG7 — Signature (optional)
    bool hasSignature = false;
    if (auto sigIdx = cardData.findGroup("signature")) {
        const auto& sigGroup = cardData.groupAt(*sigIdx);
        if (!sigGroup.fields.empty() && !sigGroup.fields[0].value.empty()) {
            const auto& value = sigGroup.fields[0].value;
            QByteArray sigRaw(reinterpret_cast<const char*>(value.data()), static_cast<qsizetype>(value.size()));
            QString sigUri = "data:image/png;base64, " + sigRaw.toBase64();
            html.replace("${signature_image}", sigUri);
            hasSignature = true;
        }
    }
    if (!hasSignature) {
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
