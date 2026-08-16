// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "emrtdtextdocument.h"
#include <plugin/fieldvalue.h>

using librecelik::plugin::fieldDetailBytes;
using librecelik::plugin::fieldValue;
using librecelik::plugin::findGroup;
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

namespace {

/// The portrait the gateway merged into the read: its own group, keyed with
/// the field half of the wire's composite key. A group that carries the image
/// under some other key still prints — the first field with bytes wins.
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

/// Bytes of a group's first field — the shape the single-image groups (DG5
/// portrait, DG7 signature) arrive in.
QByteArray firstFieldBytes(const FieldGroup& group)
{
    if (group.fields.isEmpty()) {
        return {};
    }
    return group.fields.first().detail.toByteArray();
}

} // namespace

EMRTDTextDocument::EMRTDTextDocument(const QList<FieldGroup>& groups, QString documentPath, QString cssPath)
{
    if (documentPath.isEmpty())
        documentPath = QStringLiteral(":/html/emrtdcard.html");
    if (cssPath.isEmpty())
        cssPath = QStringLiteral(":/html/emrtdcard.css");

    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, groups);

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

void EMRTDTextDocument::prepareDocumentData(QString& html, const QList<FieldGroup>& groups) const
{
    // Security status — parse from security_status group
    if (const FieldGroup* secGroup = findGroup(groups, u"security_status")) {
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

        auto integrityStr = fieldValue(*secGroup, u"overall_integrity");
        auto authenticityStr = fieldValue(*secGroup, u"overall_authenticity");
        auto genuinenessStr = fieldValue(*secGroup, u"overall_genuineness");

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
    html.replace("${surname_value}", getPreparedValue(fieldValue(groups, u"surname")));
    html.replace("${given_names_value}", getPreparedValue(fieldValue(groups, u"given_names")));
    html.replace("${nationality_value}", getPreparedValue(fieldValue(groups, u"nationality")));
    html.replace("${date_of_birth_value}", getPreparedValue(fieldValue(groups, u"date_of_birth")));
    html.replace("${sex_value}", getPreparedValue(fieldValue(groups, u"sex")));

    // Document fields
    html.replace("${document_number_value}", getPreparedValue(fieldValue(groups, u"document_number")));
    html.replace("${document_code_value}", getPreparedValue(fieldValue(groups, u"document_code")));
    html.replace("${issuing_state_value}", getPreparedValue(fieldValue(groups, u"issuing_state")));
    html.replace("${date_of_expiry_value}", getPreparedValue(fieldValue(groups, u"date_of_expiry")));
    html.replace("${personal_number_value}", getPreparedValue(fieldValue(groups, u"personal_number")));

    // Photo — raw bytes to base64 data URI
    const QByteArray raw = photoBytes(groups);
    if (!raw.isEmpty()) {
        QString dataUri = "data:image/png;base64, " + raw.toBase64();
        html.replace(":/images/user.png", dataUri);
    }

    // DG11 — Additional Personal Data (optional)
    if (const FieldGroup* dg11 = findGroup(groups, u"additional")) {
        html.replace("${full_name_value}", getPreparedValue(fieldValue(*dg11, u"full_name")));
        html.replace("${place_of_birth_value}", getPreparedValue(fieldValue(*dg11, u"place_of_birth")));
        html.replace("${address_value}", getPreparedValue(fieldValue(*dg11, u"address")));
        html.replace("${telephone_value}", getPreparedValue(fieldValue(*dg11, u"telephone")));
        html.replace("${profession_value}", getPreparedValue(fieldValue(*dg11, u"profession")));
    } else {
        removeConditionalBlock(html, "DG11");
    }

    // DG12 — Issuing Information (optional)
    if (const FieldGroup* dg12 = findGroup(groups, u"document_extra")) {
        html.replace("${issuing_authority_value}", getPreparedValue(fieldValue(*dg12, u"issuing_authority")));
        html.replace("${date_of_issue_value}", getPreparedValue(fieldValue(*dg12, u"date_of_issue")));
        html.replace("${endorsements_value}", getPreparedValue(fieldValue(*dg12, u"endorsements")));
    } else {
        removeConditionalBlock(html, "DG12");
    }

    // DG5 — Portrait (optional)
    bool hasPortrait = false;
    if (const FieldGroup* portraitGroup = findGroup(groups, u"portrait")) {
        const QByteArray portraitRaw = firstFieldBytes(*portraitGroup);
        if (!portraitRaw.isEmpty()) {
            QString portraitUri = "data:image/png;base64, " + portraitRaw.toBase64();
            html.replace("${portrait_image}", portraitUri);
            hasPortrait = true;
        }
    }
    if (!hasPortrait) {
        removeConditionalBlock(html, "DG5");
    }

    // DG16 — Contacts (optional)
    if (const FieldGroup* contactsGroup = findGroup(groups, u"contacts")) {
        html.replace("${contact_name_value}", getPreparedValue(fieldValue(*contactsGroup, u"name")));
        html.replace("${contact_telephone_value}", getPreparedValue(fieldValue(*contactsGroup, u"telephone")));
        html.replace("${contact_address_value}", getPreparedValue(fieldValue(*contactsGroup, u"address")));
    } else {
        removeConditionalBlock(html, "DG16");
    }

    // DG7 — Signature (optional)
    bool hasSignature = false;
    if (const FieldGroup* sigGroup = findGroup(groups, u"signature")) {
        const QByteArray sigRaw = firstFieldBytes(*sigGroup);
        if (!sigRaw.isEmpty()) {
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
