// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certformat.h"

#include <LibreSCRS/Certificate/ObjectIdentifier.h>

#include <QDateTime>
#include <QStringList>
#include <QTimeZone>

namespace lcc = LibreSCRS::Certificate;

namespace librecelik::certformat {

QString bytesToHex(std::span<const std::uint8_t> bytes)
{
    if (bytes.empty())
        return {};
    QString out;
    // Each byte renders as 2 hex chars + 1 colon, except the last byte has no
    // trailing colon — reserve exactly that many UTF-16 code units.
    out.reserve(static_cast<int>(bytes.size() * 3 - 1));
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i)
            out.append(QLatin1Char(':'));
        out.append(QString::asprintf("%02X", bytes[i]));
    }
    return out;
}

QString bytesToHex(const std::vector<std::uint8_t>& bytes)
{
    return bytesToHex(std::span<const std::uint8_t>(bytes.data(), bytes.size()));
}

QString bytesToHexLines(std::span<const std::uint8_t> bytes, std::size_t bytesPerLine)
{
    if (bytes.empty() || bytesPerLine == 0)
        return {};
    QString out;
    // Each byte: 2 hex chars + 1 separator (space or newline). Last byte has
    // no trailing separator.
    out.reserve(static_cast<int>(bytes.size() * 3 - 1));
    for (std::size_t i = 0; i < bytes.size(); ++i) {
        if (i && i % bytesPerLine == 0)
            out.append(QLatin1Char('\n'));
        else if (i)
            out.append(QLatin1Char(' '));
        out.append(QString::asprintf("%02X", bytes[i]));
    }
    return out;
}

QString formatTime(std::chrono::system_clock::time_point tp)
{
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(secs, QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"));
}

QString formatDate(std::chrono::system_clock::time_point tp)
{
    const auto secs = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    return QDateTime::fromSecsSinceEpoch(secs, QTimeZone::UTC).toString(QStringLiteral("dd.MM.yyyy"));
}

QString keyUsageBitLabel(lcc::KeyUsageBit bit)
{
    switch (bit) {
    case lcc::KeyUsageBit::DigitalSignature:
        return qtTrId("lc-token-ku-digital-signature");
    case lcc::KeyUsageBit::NonRepudiation:
        return qtTrId("lc-token-ku-non-repudiation");
    case lcc::KeyUsageBit::KeyEncipherment:
        return qtTrId("lc-token-ku-key-encipherment");
    case lcc::KeyUsageBit::DataEncipherment:
        return qtTrId("lc-token-ku-data-encipherment");
    case lcc::KeyUsageBit::KeyAgreement:
        return qtTrId("lc-token-ku-key-agreement");
    case lcc::KeyUsageBit::KeyCertSign:
        return qtTrId("lc-cert-ku-key-cert-sign");
    case lcc::KeyUsageBit::CRLSign:
        return qtTrId("lc-cert-ku-crl-sign");
    case lcc::KeyUsageBit::EncipherOnly:
        return qtTrId("lc-cert-ku-encipher-only");
    case lcc::KeyUsageBit::DecipherOnly:
        return qtTrId("lc-cert-ku-decipher-only");
    }
    return {};
}

QString keyUsageToString(std::span<const lcc::KeyUsageBit> bits)
{
    QStringList parts;
    for (auto bit : bits)
        parts << keyUsageBitLabel(bit);
    return parts.join(QStringLiteral(", "));
}

QString keyUsageToStringEndEntity(std::span<const lcc::KeyUsageBit> bits)
{
    // Token section's UI focuses on end-entity capability indicators; the
    // CA / EncipherOnly / DecipherOnly bits are intentionally not surfaced
    // here. The certificate viewer dialog shows the complete list.
    QStringList parts;
    for (auto bit : bits) {
        switch (bit) {
        case lcc::KeyUsageBit::DigitalSignature:
        case lcc::KeyUsageBit::NonRepudiation:
        case lcc::KeyUsageBit::KeyEncipherment:
        case lcc::KeyUsageBit::DataEncipherment:
        case lcc::KeyUsageBit::KeyAgreement:
            parts << keyUsageBitLabel(bit);
            break;
        case lcc::KeyUsageBit::KeyCertSign:
        case lcc::KeyUsageBit::CRLSign:
        case lcc::KeyUsageBit::EncipherOnly:
        case lcc::KeyUsageBit::DecipherOnly:
            break;
        }
    }
    return parts.join(QStringLiteral(", "));
}

QString generalNameTypeLabel(lcc::GeneralNameType t)
{
    switch (t) {
    case lcc::GeneralNameType::Rfc822Name:
        return qtTrId("lc-cert-gn-rfc822");
    case lcc::GeneralNameType::DnsName:
        return qtTrId("lc-cert-gn-dns");
    case lcc::GeneralNameType::UniformResourceIdentifier:
        return qtTrId("lc-cert-gn-uri");
    case lcc::GeneralNameType::IpAddress:
        return qtTrId("lc-cert-gn-ip");
    case lcc::GeneralNameType::DirectoryName:
        return qtTrId("lc-cert-gn-directory");
    case lcc::GeneralNameType::RegisteredId:
        return qtTrId("lc-cert-gn-registered-id");
    case lcc::GeneralNameType::OtherName:
        return qtTrId("lc-cert-gn-other");
    case lcc::GeneralNameType::X400Address:
        return qtTrId("lc-cert-gn-x400");
    case lcc::GeneralNameType::EdiPartyName:
        return qtTrId("lc-cert-gn-edi");
    }
    return qtTrId("lc-cert-gn-other");
}

namespace {

/// Bare algorithm label (no curve, no bit length) used by both
/// publicKeyAlgorithmLabel and publicKeyDescription.
QString algorithmBaseLabel(const lcc::PublicKeyInfo& pk)
{
    switch (pk.algorithm) {
    case lcc::PublicKeyAlgorithm::RSA:
        return QStringLiteral("RSA");
    case lcc::PublicKeyAlgorithm::ECDSA:
        return QStringLiteral("ECDSA");
    case lcc::PublicKeyAlgorithm::EdDSA:
        return QStringLiteral("EdDSA");
    case lcc::PublicKeyAlgorithm::Other:
        if (!pk.algorithmDescription.empty())
            return QString::fromStdString(pk.algorithmDescription);
        if (!pk.algorithmOid.dottedDecimal.empty()) {
            const std::string friendly = pk.algorithmOid.friendlyName();
            return friendly.empty() ? QString::fromStdString(pk.algorithmOid.dottedDecimal)
                                    : QString::fromStdString(friendly);
        }
        return qtTrId("lc-cert-algorithm-unknown");
    }
    return qtTrId("lc-cert-algorithm-unknown");
}

/// Render a curve OID — friendly name when known (the OID DB currently
/// doesn't carry curve entries, but the indirection keeps the door open),
/// dotted decimal otherwise. Returns empty for an empty input.
QString renderCurve(const std::string& curveOid)
{
    if (curveOid.empty())
        return {};
    lcc::ObjectIdentifier oid(curveOid);
    const std::string friendly = oid.friendlyName();
    return friendly.empty() ? QString::fromStdString(curveOid) : QString::fromStdString(friendly);
}

} // namespace

QString publicKeyAlgorithmLabel(const lcc::PublicKeyInfo& pk)
{
    QString label = algorithmBaseLabel(pk);
    QString curve = renderCurve(pk.curveOid);
    if (!curve.isEmpty())
        label += QStringLiteral(" (%1)").arg(curve);
    return label;
}

QString publicKeyDescription(const lcc::PublicKeyInfo& pk)
{
    QString algo = algorithmBaseLabel(pk);
    QString desc = QStringLiteral("%1 %2-bit").arg(algo).arg(pk.bitLength);
    QString curve = renderCurve(pk.curveOid);
    if (!curve.isEmpty())
        desc += QStringLiteral(" (%1)").arg(curve);
    return desc;
}

} // namespace librecelik::certformat
