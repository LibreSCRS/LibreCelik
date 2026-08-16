// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificatepropertiesmodel.h"
#include "certfields.h"
#include "certformat.h"
#include "certificateinfoitem.h"

#include <QLatin1StringView>
#include <QString>
#include <QStringList>

namespace cf = librecelik::certformat;
namespace fields = librecelik::certfields;

using LibreSCRS::AgentClient::CertificateInfo;

namespace {

// Group keys of the agent's certificate field dictionary. Frozen wire
// spellings — see the client library's CertificateInfo documentation.
constexpr QLatin1StringView kGroupSubject{"subject"};
constexpr QLatin1StringView kGroupIssuer{"issuer"};
constexpr QLatin1StringView kGroupPublicKey{"publicKey"};
constexpr QLatin1StringView kGroupCert{"cert"};
constexpr QLatin1StringView kGroupBasicConstraints{"basicConstraints"};
constexpr QLatin1StringView kGroupSan{"san"};
constexpr QLatin1StringView kGroupIan{"ian"};
constexpr QLatin1StringView kGroupCrlDp{"crlDp"};
constexpr QLatin1StringView kGroupAia{"aia"};
constexpr QLatin1StringView kGroupPolicies{"certificatePolicies"};
constexpr QLatin1StringView kGroupExt{"ext"};
constexpr QLatin1StringView kGroupEku{"eku"};

// Extension NAMES for the groups the agent gives a dedicated group to. The
// wire carries a label for every cell but none for a group, so these are
// spelled here, matching the agent's own group labels word for word. They are
// technical X.509 names and stay untranslated — exactly as they were when
// they came out of the middleware's OID database.
constexpr QLatin1StringView kExtKeyUsage{"Key Usage"};
constexpr QLatin1StringView kExtExtendedKeyUsage{"Extended Key Usage"};
constexpr QLatin1StringView kExtSubjectAltName{"Subject Alternative Name"};
constexpr QLatin1StringView kExtIssuerAltName{"Issuer Alternative Name"};
constexpr QLatin1StringView kExtBasicConstraints{"Basic Constraints"};
constexpr QLatin1StringView kExtSubjectKeyId{"Subject Key Identifier"};
constexpr QLatin1StringView kExtAuthorityKeyId{"Authority Key Identifier"};
constexpr QLatin1StringView kExtCrlDp{"CRL Distribution Points"};
constexpr QLatin1StringView kExtAia{"Authority Information Access"};
constexpr QLatin1StringView kExtCertificatePolicies{"Certificate Policies"};

/// One row of the Extensions subtree.
struct ExtensionRow
{
    QString label;
    QString value;
    bool critical = false;
};

/// Leading non-digit part of an ordinal-keyed field ("dns1" -> "dns",
/// "caIssuers0" -> "caIssuers").
[[nodiscard]] QString fieldToken(const QString& fieldKey)
{
    QString token = fieldKey;
    while (!token.isEmpty() && token.back().isDigit())
        token.chop(1);
    return token;
}

/// Localised GeneralName type label for a SAN/IAN entry, keyed off the type
/// token the agent puts in front of the entry's ordinal. An unrecognised
/// token falls back to the agent's own English type name, which rides the
/// cell — the token vocabulary can grow without this build.
[[nodiscard]] QString generalNameTypeLabel(const QString& fieldKey, const fields::Cell& cell)
{
    const QString token = fieldToken(fieldKey);
    if (token == QLatin1StringView("email"))
        return qtTrId("lc-cert-gn-rfc822");
    if (token == QLatin1StringView("dns"))
        return qtTrId("lc-cert-gn-dns");
    if (token == QLatin1StringView("uri"))
        return qtTrId("lc-cert-gn-uri");
    if (token == QLatin1StringView("ip"))
        return qtTrId("lc-cert-gn-ip");
    if (token == QLatin1StringView("directory"))
        return qtTrId("lc-cert-gn-directory");
    if (token == QLatin1StringView("registeredId"))
        return qtTrId("lc-cert-gn-registered-id");
    if (token == QLatin1StringView("otherName"))
        return qtTrId("lc-cert-gn-other");
    if (token == QLatin1StringView("x400Address"))
        return qtTrId("lc-cert-gn-x400");
    if (token == QLatin1StringView("ediPartyName"))
        return qtTrId("lc-cert-gn-edi");
    return cell.labelFallback;
}

/// "<Type>: <value>" per entry, one entry per line — the rendering the
/// alternative-name panes have always used.
[[nodiscard]] QString generalNamesToString(const QVariantMap& group)
{
    QStringList parts;
    for (const auto& [key, cell] : fields::orderedCells(group)) {
        if (cell.isEmpty())
            continue;
        parts << QStringLiteral("%1: %2").arg(generalNameTypeLabel(key, cell), cell.value);
    }
    return parts.join(QLatin1Char('\n'));
}

/// Every non-empty value of a group, in the agent's order, joined by
/// @p separator.
[[nodiscard]] QString joinedValues(const QVariantMap& group, const QString& separator)
{
    QStringList parts;
    for (const auto& [key, cell] : fields::orderedCells(group)) {
        Q_UNUSED(key);
        if (!cell.isEmpty())
            parts << cell.value;
    }
    return parts.join(separator);
}

void appendField(CertificateInfoItem* parent, const QString& label, const QString& value)
{
    parent->appendChild(std::make_unique<CertificateInfoItem>(label, value, parent));
}

/// Public-key line: the algorithm the agent resolved, with the curve appended
/// in parentheses when the certificate carries one. An agent that resolved no
/// algorithm at all leaves the row honestly unknown rather than inventing a
/// name from an OID this process cannot look up.
[[nodiscard]] QString publicKeyAlgorithmText(const QVariantMap& groups)
{
    QString algorithm = fields::valueOf(groups, kGroupPublicKey, QLatin1StringView("algorithm"));
    if (algorithm.isEmpty())
        algorithm = qtTrId("lc-cert-algorithm-unknown");
    const QString curve = fields::valueOf(groups, kGroupPublicKey, QLatin1StringView("curveOid"));
    if (curve.isEmpty())
        return algorithm;
    return QStringLiteral("%1 (%2)").arg(algorithm, curve);
}

/// Extended key usages by name. The agent resolves them against its own OID
/// database and ships the names in the `eku` group; the typed dotted OIDs are
/// the fallback for an agent that predates that group.
[[nodiscard]] QString extendedKeyUsageText(const CertificateInfo& cert, const QVariantMap& groups)
{
    const QVariantMap group = fields::groupOf(groups, kGroupEku);
    if (!group.isEmpty())
        return joinedValues(group, QStringLiteral(", "));

    QStringList parts;
    parts.reserve(cert.extendedKeyUsageOids.size());
    for (const QString& oid : cert.extendedKeyUsageOids)
        parts << cf::extendedKeyUsageLabel(oid);
    return parts.join(QStringLiteral(", "));
}

[[nodiscard]] QString basicConstraintsText(const QVariantMap& groups)
{
    const QVariantMap group = fields::groupOf(groups, kGroupBasicConstraints);
    if (group.isEmpty())
        return {};
    QStringList parts;
    const QString isCa = fields::toCell(group.value(QStringLiteral("isCa"))).value;
    parts << (isCa == QLatin1StringView("true") ? QStringLiteral("CA: TRUE") : QStringLiteral("CA: FALSE"));
    const QString pathLen = fields::toCell(group.value(QStringLiteral("pathLen"))).value;
    if (!pathLen.isEmpty())
        parts << QStringLiteral("Path Length: %1").arg(pathLen);
    return parts.join(QStringLiteral(", "));
}

/// Authority Information Access: OCSP responders first, then CA issuers —
/// the order this pane has always shown them in. Their ordinals restart per
/// kind, so the group's own ordering cannot express it.
[[nodiscard]] QString authorityInfoAccessText(const QVariantMap& groups)
{
    const QVariantMap group = fields::groupOf(groups, kGroupAia);
    if (group.isEmpty())
        return {};
    const auto cells = fields::orderedCells(group);
    QStringList ocsp;
    QStringList caIssuers;
    QStringList other;
    for (const auto& [key, cell] : cells) {
        if (cell.isEmpty())
            continue;
        const QString token = fieldToken(key);
        if (token == QLatin1StringView("ocsp"))
            ocsp << QStringLiteral("OCSP: %1").arg(cell.value);
        else if (token == QLatin1StringView("caIssuers"))
            caIssuers << QStringLiteral("CA Issuers: %1").arg(cell.value);
        else
            other << QStringLiteral("%1: %2").arg(cell.labelFallback, cell.value);
    }
    return (ocsp + caIssuers + other).join(QLatin1Char('\n'));
}

} // namespace

CertificatePropertiesModel::CertificatePropertiesModel(const CertificateInfo& cert, QObject* parent)
    : CertificateTreeViewModel(parent)
{
    if (fields::isUnparseable(cert))
        addParseError(cert, {});
    else
        buildTree(cert);
}

CertificatePropertiesModel::CertificatePropertiesModel(const CertificateInfo& cert, QByteArrayView forensicDer,
                                                       QObject* parent)
    : CertificateTreeViewModel(parent)
{
    if (fields::isUnparseable(cert))
        addParseError(cert, forensicDer);
    else
        buildTree(cert);
}

void CertificatePropertiesModel::addParseError(const CertificateInfo& cert, QByteArrayView forensicDer)
{
    // clang-format off
    const auto t = qtTrId("lc-cert-parse-error"); // i18n-audit: ignore D1, item-view model retranslates via Qt::DisplayRole on next paint
    // clang-format on
    rootItem->appendChild(std::make_unique<CertificateInfoItem>(t, fields::parseErrorDetail(cert), rootItem.get()));

    if (forensicDer.isEmpty())
        return;

    // Forensic hex dump: 16 bytes per line, space-separated upper-case.
    // "DER" is a non-localised technical acronym (Distinguished Encoding Rules);
    // the dump itself is raw data, never translated text.
    rootItem->appendChild(
        std::make_unique<CertificateInfoItem>(QStringLiteral("DER"), cf::bytesToHexLines(forensicDer), rootItem.get()));
}

void CertificatePropertiesModel::buildTree(const CertificateInfo& cert)
{
    auto* root = rootItem.get();
    const QVariantMap groups = fields::groupsOf(cert);

    // Version
    appendField(root, qtTrId("lc-cert-field-version"),
                fields::valueOf(groups, kGroupCert, QLatin1StringView("version")));

    // Serial number
    appendField(root, qtTrId("lc-cert-field-serial-number"),
                fields::valueOf(groups, kGroupCert, QLatin1StringView("serial")));

    // Signature algorithm
    appendField(root, qtTrId("lc-cert-field-signature-algo"),
                fields::valueOf(groups, kGroupCert, QLatin1StringView("signatureAlgorithm")));

    // Issuer / subject: the distinguished name as the agent rendered it. The
    // typed member is the common name alone, so it only stands in when the
    // field dictionary is missing entirely.
    {
        QString dn = fields::valueOf(groups, kGroupIssuer, QLatin1StringView("dn"));
        if (dn.isEmpty())
            dn = cert.issuer;
        appendField(root, qtTrId("lc-cert-field-issuer"), dn);
    }

    // Validity
    {
        auto validityItem = std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-field-validity"), QString(), root);
        auto* vp = validityItem.get();
        appendField(vp, qtTrId("lc-cert-field-not-before"), cf::formatTime(cert.notBefore));
        appendField(vp, qtTrId("lc-cert-field-not-after"), cf::formatTime(cert.notAfter));
        root->appendChild(std::move(validityItem));
    }

    {
        QString dn = fields::valueOf(groups, kGroupSubject, QLatin1StringView("dn"));
        if (dn.isEmpty())
            dn = cert.subject;
        appendField(root, qtTrId("lc-cert-field-subject"), dn);
    }

    // Public key
    {
        auto pkItem = std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-field-public-key-info"), QString(), root);
        auto* pkPtr = pkItem.get();
        appendField(pkPtr, qtTrId("lc-cert-field-algorithm"), publicKeyAlgorithmText(groups));
        const QString sizeBits = fields::valueOf(groups, kGroupPublicKey, QLatin1StringView("sizeBits"));
        if (!sizeBits.isEmpty())
            appendField(pkPtr, qtTrId("lc-cert-field-key-size"), QStringLiteral("%1 bits").arg(sizeBits));
        root->appendChild(std::move(pkItem));
    }

    // Extensions. The count is re-derived from the rows actually rendered —
    // the wire carries no extension total, and a count that disagreed with
    // the list under it would be worse than none.
    QList<ExtensionRow> rows;

    if (cert.keyUsageBits != 0)
        rows.append({QString(kExtKeyUsage), cf::keyUsageToString(cert.keyUsageBits), false});

    if (const QString eku = extendedKeyUsageText(cert, groups); !eku.isEmpty())
        rows.append({QString(kExtExtendedKeyUsage), eku, fields::groupIsCritical(groups, kGroupEku)});

    if (const QString san = generalNamesToString(fields::groupOf(groups, kGroupSan)); !san.isEmpty())
        rows.append({QString(kExtSubjectAltName), san, fields::groupIsCritical(groups, kGroupSan)});

    if (const QString ian = generalNamesToString(fields::groupOf(groups, kGroupIan)); !ian.isEmpty())
        rows.append({QString(kExtIssuerAltName), ian, fields::groupIsCritical(groups, kGroupIan)});

    if (const QString bc = basicConstraintsText(groups); !bc.isEmpty())
        rows.append({QString(kExtBasicConstraints), bc, fields::groupIsCritical(groups, kGroupBasicConstraints)});

    // The two key identifiers are cells of the `cert` group rather than groups
    // of their own, so their criticality rides the cell label; every extension
    // above reads it from its group's own `critical` metadata cell. Two
    // carriers, one rendering — the localized suffix plus the bold row.
    if (const auto ski = fields::cellOf(groups, kGroupCert, QLatin1StringView("subjectKeyIdentifier")); !ski.isEmpty())
        rows.append({QString(kExtSubjectKeyId), ski.value, ski.critical});

    if (const auto aki = fields::cellOf(groups, kGroupCert, QLatin1StringView("authorityKeyIdentifier"));
        !aki.isEmpty())
        rows.append({QString(kExtAuthorityKeyId), aki.value, aki.critical});

    if (const QString crl = joinedValues(fields::groupOf(groups, kGroupCrlDp), QStringLiteral("\n")); !crl.isEmpty())
        rows.append({QString(kExtCrlDp), crl, fields::groupIsCritical(groups, kGroupCrlDp)});

    if (const QString aia = authorityInfoAccessText(groups); !aia.isEmpty())
        rows.append({QString(kExtAia), aia, fields::groupIsCritical(groups, kGroupAia)});

    if (const QString pol = joinedValues(fields::groupOf(groups, kGroupPolicies), QStringLiteral(", ")); !pol.isEmpty())
        rows.append({QString(kExtCertificatePolicies), pol, fields::groupIsCritical(groups, kGroupPolicies)});

    // Every extension the agent did not give a group of its own: its label is
    // the agent's OID name (dotted when it has none) and its value the raw
    // extension bytes, exactly as this pane has always shown an unrecognised
    // extension.
    for (const auto& [key, cell] : fields::orderedCells(fields::groupOf(groups, kGroupExt))) {
        Q_UNUSED(key);
        rows.append({cell.labelFallback, cell.value, cell.critical});
    }

    if (rows.isEmpty())
        return;

    auto extRoot = std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-field-extensions"),
                                                         QStringLiteral("(%1)").arg(rows.size()), root);
    auto* extPtr = extRoot.get();
    for (const ExtensionRow& row : rows) {
        QString label = row.label;
        if (row.critical)
            label += qtTrId("lc-cert-extension-critical");
        extPtr->appendChild(std::make_unique<CertificateInfoItem>(label, row.value, row.critical, extPtr));
    }
    root->appendChild(std::move(extRoot));
}
