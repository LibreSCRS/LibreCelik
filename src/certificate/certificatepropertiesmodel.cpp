// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certificatepropertiesmodel.h"
#include "cert_format.h"
#include "certificateinfoitem.h"

#include <LibreSCRS/Certificate/ParsedCertificate.h>

#include <QString>
#include <QStringList>

#include <cstdint>
#include <span>
#include <string>

namespace lcc = LibreSCRS::Certificate;
namespace cf = librecelik::cert_format;

namespace {

QString fromStd(const std::string& s)
{
    return QString::fromStdString(s);
}

QString formatDistinguishedName(const lcc::DistinguishedName& dn)
{
    QStringList lines;
    for (const auto& comp : dn.components) {
        const QString name =
            comp.oid.friendlyName().empty() ? fromStd(comp.oid.dottedDecimal) : fromStd(comp.oid.friendlyName());
        lines << QStringLiteral("%1=%2").arg(name, fromStd(comp.value));
    }
    return lines.join(QLatin1Char('\n'));
}

QString oidLabel(const lcc::ObjectIdentifier& oid)
{
    if (!oid.friendlyName().empty())
        return fromStd(oid.friendlyName());
    return fromStd(oid.dottedDecimal);
}

QString generalNamesToString(const std::vector<lcc::GeneralName>& names)
{
    QStringList parts;
    for (const auto& n : names) {
        if (!n.value.empty())
            parts << QStringLiteral("%1: %2").arg(cf::generalNameTypeLabel(n.type), fromStd(n.value));
        else
            parts << QStringLiteral("%1: %2").arg(cf::generalNameTypeLabel(n.type), cf::bytesToHex(n.rawValue));
    }
    return parts.join(QLatin1Char('\n'));
}

void appendField(CertificateInfoItem* parent, const QString& label, const QString& value)
{
    parent->appendChild(std::make_unique<CertificateInfoItem>(label, value, parent));
}

} // namespace

CertificatePropertiesModel::CertificatePropertiesModel(const lcc::ParsedCertificate* cert, QObject* parent)
    : CertificateTreeViewModel(parent)
{
    if (cert)
        buildTree(*cert);
    else
        addParseError({});
}

CertificatePropertiesModel::CertificatePropertiesModel(const lcc::ParsedCertificate* cert,
                                                       std::span<const std::uint8_t> rawDer, QObject* parent)
    : CertificateTreeViewModel(parent)
{
    if (cert)
        buildTree(*cert);
    else
        addParseError(rawDer);
}

void CertificatePropertiesModel::addParseError(std::span<const std::uint8_t> rawDer)
{
    rootItem->appendChild(
        std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-parse-error"), QString(), rootItem.get()));

    if (rawDer.empty())
        return;

    // Forensic hex dump: 16 bytes per line, space-separated upper-case.
    // "DER" is a non-localised technical acronym (Distinguished Encoding Rules);
    // the dump itself is raw data, never translated text.
    rootItem->appendChild(
        std::make_unique<CertificateInfoItem>(QStringLiteral("DER"), cf::bytesToHexLines(rawDer), rootItem.get()));
}

void CertificatePropertiesModel::buildTree(const lcc::ParsedCertificate& cert)
{
    auto* root = rootItem.get();

    // Version
    appendField(root, qtTrId("lc-cert-field-version"), QStringLiteral("V%1").arg(cert.version()));

    // Serial number
    appendField(root, qtTrId("lc-cert-field-serial-number"), cf::bytesToHex(cert.serialNumber()));

    // Signature algorithm
    QString sigAlgo = fromStd(cert.signatureAlgorithmDescription());
    if (sigAlgo.isEmpty())
        sigAlgo = oidLabel(cert.signatureAlgorithmOid());
    appendField(root, qtTrId("lc-cert-field-signature-algo"), sigAlgo);

    // Issuer
    appendField(root, qtTrId("lc-cert-field-issuer"), formatDistinguishedName(cert.issuer()));

    // Validity
    {
        auto validityItem = std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-field-validity"), QString(), root);
        auto* vp = validityItem.get();
        appendField(vp, qtTrId("lc-cert-field-not-before"), cf::formatTime(cert.notBefore()));
        appendField(vp, qtTrId("lc-cert-field-not-after"), cf::formatTime(cert.notAfter()));
        root->appendChild(std::move(validityItem));
    }

    // Subject
    appendField(root, qtTrId("lc-cert-field-subject"), formatDistinguishedName(cert.subject()));

    // Public key
    {
        const auto pk = cert.publicKey();
        auto pkItem = std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-field-public-key-info"), QString(), root);
        auto* pkPtr = pkItem.get();
        // publicKeyAlgorithmLabel handles the algorithm-name fallbacks
        // (description → friendly OID → dotted OID → "Unknown") and appends
        // the curve OID in parentheses for ECDSA, so no separate Curve row is
        // needed (single source of truth for the public-key textual rendering).
        appendField(pkPtr, qtTrId("lc-cert-field-algorithm"), cf::publicKeyAlgorithmLabel(pk));
        appendField(pkPtr, qtTrId("lc-cert-field-key-size"), QStringLiteral("%1 bits").arg(pk.bitLength));
        root->appendChild(std::move(pkItem));
    }

    // Extensions
    const auto exts = cert.extensions();
    if (exts.empty())
        return;

    auto extRoot = std::make_unique<CertificateInfoItem>(qtTrId("lc-cert-field-extensions"),
                                                         QStringLiteral("(%1)").arg(exts.size()), root);
    auto* extPtr = extRoot.get();

    auto addExt = [&](const QString& label, const QString& value, bool critical) {
        QString fullLabel = label;
        if (critical)
            fullLabel += qtTrId("lc-cert-extension-critical");
        extPtr->appendChild(std::make_unique<CertificateInfoItem>(fullLabel, value, critical, extPtr));
    };

    for (const auto& ext : exts) {
        const QString label = oidLabel(ext.oid);

        // Look up typed accessor for this OID, fall back to raw hex.
        const std::string& oidDot = ext.oid.dottedDecimal;
        QString value;

        if (oidDot == "2.5.29.15") {
            // Key Usage
            if (auto ku = cert.keyUsage())
                value = cf::keyUsageToString(*ku);
        } else if (oidDot == "2.5.29.37") {
            // Extended Key Usage
            if (auto eku = cert.extendedKeyUsage()) {
                QStringList parts;
                for (const auto& oid : *eku)
                    parts << oidLabel(oid);
                value = parts.join(QStringLiteral(", "));
            }
        } else if (oidDot == "2.5.29.17") {
            if (auto san = cert.subjectAlternativeNames())
                value = generalNamesToString(*san);
        } else if (oidDot == "2.5.29.18") {
            if (auto ian = cert.issuerAlternativeNames())
                value = generalNamesToString(*ian);
        } else if (oidDot == "2.5.29.19") {
            if (auto bc = cert.basicConstraints()) {
                QStringList parts;
                parts << (bc->isCa ? QStringLiteral("CA: TRUE") : QStringLiteral("CA: FALSE"));
                if (bc->pathLenConstraint)
                    parts << QStringLiteral("Path Length: %1").arg(*bc->pathLenConstraint);
                value = parts.join(QStringLiteral(", "));
            }
        } else if (oidDot == "2.5.29.35") {
            if (auto aki = cert.authorityKeyIdentifier())
                value = cf::bytesToHex(*aki);
        } else if (oidDot == "2.5.29.14") {
            if (auto ski = cert.subjectKeyIdentifier())
                value = cf::bytesToHex(*ski);
        } else if (oidDot == "2.5.29.31") {
            if (auto crl = cert.crlDistributionPoints()) {
                QStringList parts;
                for (const auto& url : *crl)
                    parts << fromStd(url);
                value = parts.join(QLatin1Char('\n'));
            }
        } else if (oidDot == "1.3.6.1.5.5.7.1.1") {
            QStringList parts;
            if (auto ocsp = cert.ocspResponderUrls())
                for (const auto& url : *ocsp)
                    parts << QStringLiteral("OCSP: %1").arg(fromStd(url));
            if (auto ca = cert.caIssuersUrls())
                for (const auto& url : *ca)
                    parts << QStringLiteral("CA Issuers: %1").arg(fromStd(url));
            value = parts.join(QLatin1Char('\n'));
        } else if (oidDot == "2.5.29.32") {
            if (auto pol = cert.certificatePolicies()) {
                QStringList parts;
                for (const auto& oid : *pol)
                    parts << oidLabel(oid);
                value = parts.join(QStringLiteral(", "));
            }
        }

        if (value.isEmpty())
            value = cf::bytesToHex(ext.value);

        addExt(label, value, ext.critical);
    }

    root->appendChild(std::move(extRoot));
}
