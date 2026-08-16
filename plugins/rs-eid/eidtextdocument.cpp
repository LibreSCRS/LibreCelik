// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include <QCoreApplication>
#include <QDate>
#include "eidtextdocument.h"
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

} // namespace

EIdTextDocument::EIdTextDocument(const QList<FieldGroup>& groups, QString documentPath, QString cssPath)
{
    // Foreigner detection — matches EidWidget::isForeigner()
    isForeigner = fieldValue(groups, u"card_type") == QLatin1String("ForeignerIF2020");

    if (documentPath.isEmpty())
        documentPath = isForeigner ? QStringLiteral(":/html/idcardIF2020.html") : QStringLiteral(":/html/idcard.html");
    if (cssPath.isEmpty())
        cssPath = QStringLiteral(":/html/idcard.css");

    auto data = loadFile(documentPath);

    translateDocumentData(data);
    prepareDocumentData(data, groups);

    setupDocument(data, cssPath);
}

void EIdTextDocument::translateDocumentData(QString& data) const
{
    data.replace(
        "${title}",
        qtTrId("lc-eid-doc-title")); // i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
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

void EIdTextDocument::prepareDocumentData(QString& html, const QList<FieldGroup>& groups) const
{
    html.replace("${last_name_value}", getPreparedValue(fieldValue(groups, u"surname")));
    html.replace("${first_name_value}", getPreparedValue(fieldValue(groups, u"given_name")));
    html.replace("${parent_name_value}", getPreparedValue(fieldValue(groups, u"parent_given_name")));
    html.replace("${nationality_value}", getPreparedValue(fieldValue(groups, u"nationality")));
    html.replace("${date_of_birth_value}", getPreparedValue(fieldValue(groups, u"date_of_birth")));

    // Composite: place of birth
    QStringList pobParts;
    pobParts << fieldValue(groups, u"place_of_birth") << fieldValue(groups, u"community_of_birth")
             << fieldValue(groups, u"state_of_birth");
    pobParts.removeAll(QString());
    html.replace("${place_of_birth_value}", getPreparedValue(pobParts.join(", ")));

    html.replace("${status_of_foreigner_value}", getPreparedValue(fieldValue(groups, u"status_of_foreigner")));

    // Composite: address (same assembly as EidWidget::buildAddressSection)
    QStringList addrParts;
    addrParts << fieldValue(groups, u"state") << fieldValue(groups, u"community") << fieldValue(groups, u"place")
              << fieldValue(groups, u"street") << fieldValue(groups, u"house_number");
    auto houseLetter = fieldValue(groups, u"house_letter");
    if (!houseLetter.isEmpty())
        addrParts << houseLetter;
    addrParts.removeAll(QString());
    auto address = addrParts.join(", ");
    auto entrance = fieldValue(groups, u"entrance");
    auto floor = fieldValue(groups, u"floor");
    auto apartment = fieldValue(groups, u"apartment_number");
    if (!entrance.isEmpty())
        address += "/" + entrance;
    if (!floor.isEmpty())
        address += "/" + floor;
    if (!apartment.isEmpty())
        address += "/" + apartment;
    html.replace("${adress_value}", getPreparedValue(address));

    html.replace("${date_of_address_change_value}", getPreparedValue(fieldValue(groups, u"address_date")));
    html.replace("${jmbg_value}", getPreparedValue(fieldValue(groups, u"personal_number")));
    html.replace("${gender_value}", getPreparedValue(fieldValue(groups, u"sex")));

    html.replace("${document_issuer_value}", getPreparedValue(fieldValue(groups, u"issuing_authority")));
    html.replace("${document_number_value}", getPreparedValue(fieldValue(groups, u"doc_reg_no")));
    html.replace("${issuance_date_value}", getPreparedValue(fieldValue(groups, u"issuing_date")));
    html.replace("${validity_date_value}", getPreparedValue(fieldValue(groups, u"expiry_date")));

    // Photo — raw bytes to base64 data URI
    const QByteArray raw = photoBytes(groups);
    if (!raw.isEmpty()) {
        QString dataUri = "data:image/png;base64, " + raw.toBase64();
        html.replace(":/images/user.png", dataUri);
    }
}
