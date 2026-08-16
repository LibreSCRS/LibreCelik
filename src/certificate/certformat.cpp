// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "certformat.h"

#include <QDateTime>
#include <QStringList>
#include <QTimeZone>

namespace librecelik::certformat {

namespace {

/// RFC 5280 §4.2.1.3 ordinals, in the order the bitmask numbers them. The
/// index into this table IS the bit index — the mask the agent sends is the
/// RFC's own numbering, forwarded verbatim.
constexpr int kKeyUsageBitCount = 9;

/// Highest ordinal the token summary renders. keyCertSign (5) and everything
/// above it describe CA capability or cipher-direction restrictions, which
/// that summary deliberately leaves to the full certificate viewer.
constexpr int kEndEntityBitCount = 5;

QString hex(QByteArrayView bytes, char separator, qsizetype bytesPerLine)
{
    if (bytes.isEmpty())
        return {};
    QString out;
    // Each byte renders as 2 hex chars + 1 separator, except the last byte has
    // no trailing separator — reserve exactly that many UTF-16 code units.
    out.reserve(static_cast<qsizetype>(bytes.size() * 3 - 1));
    for (qsizetype i = 0; i < bytes.size(); ++i) {
        if (i && bytesPerLine > 0 && i % bytesPerLine == 0)
            out.append(QLatin1Char('\n'));
        else if (i)
            out.append(QLatin1Char(separator));
        out.append(QString::asprintf("%02X", static_cast<unsigned char>(bytes[i])));
    }
    return out;
}

} // namespace

QString bytesToHex(QByteArrayView bytes)
{
    return hex(bytes, ':', /*bytesPerLine=*/0);
}

QString bytesToHexLines(QByteArrayView bytes, qsizetype bytesPerLine)
{
    if (bytesPerLine <= 0)
        return {};
    return hex(bytes, ' ', bytesPerLine);
}

QString formatTime(const QDateTime& tp)
{
    if (!tp.isValid())
        return {};
    return tp.toTimeZone(QTimeZone::UTC).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss 'UTC'"));
}

QString formatDate(const QDateTime& tp)
{
    if (!tp.isValid())
        return {};
    return tp.toTimeZone(QTimeZone::UTC).toString(QStringLiteral("dd.MM.yyyy"));
}

QString keyUsageBitLabel(int bitIndex)
{
    switch (bitIndex) {
    case 0:
        return qtTrId("lc-token-ku-digital-signature");
    case 1:
        return qtTrId("lc-token-ku-non-repudiation");
    case 2:
        return qtTrId("lc-token-ku-key-encipherment");
    case 3:
        return qtTrId("lc-token-ku-data-encipherment");
    case 4:
        return qtTrId("lc-token-ku-key-agreement");
    case 5:
        return qtTrId("lc-cert-ku-key-cert-sign");
    case 6:
        return qtTrId("lc-cert-ku-crl-sign");
    case 7:
        return qtTrId("lc-cert-ku-encipher-only");
    case 8:
        return qtTrId("lc-cert-ku-decipher-only");
    default:
        break;
    }
    return {};
}

namespace {

/// Shared body of the two KeyUsage renderings: every set bit below @p bitCount
/// that this build has a label for, in ascending ordinal order. A set bit with
/// no label contributes nothing at all — never an empty fragment between two
/// separators.
QString renderKeyUsage(quint32 keyUsageBits, int bitCount)
{
    QStringList parts;
    for (int bit = 0; bit < bitCount; ++bit) {
        if ((keyUsageBits & (1u << static_cast<quint32>(bit))) == 0)
            continue;
        const QString label = keyUsageBitLabel(bit);
        if (!label.isEmpty())
            parts << label;
    }
    return parts.join(QStringLiteral(", "));
}

} // namespace

QString keyUsageToString(quint32 keyUsageBits)
{
    return renderKeyUsage(keyUsageBits, kKeyUsageBitCount);
}

QString keyUsageToStringEndEntity(quint32 keyUsageBits)
{
    // Token section's UI focuses on end-entity capability indicators; the
    // CA / EncipherOnly / DecipherOnly bits are intentionally not surfaced
    // here. The certificate viewer dialog shows the complete list.
    return renderKeyUsage(keyUsageBits, kEndEntityBitCount);
}

QString extendedKeyUsageLabel(const QString& dottedOid)
{
    // RFC 5280 §4.2.1.12's own set, spelled as the agent's OID database
    // spells them so the two paths read identically. Deliberately short: see
    // the header — this is the older-agent fallback, not a name database.
    if (dottedOid == QLatin1StringView("1.3.6.1.5.5.7.3.1"))
        return QStringLiteral("TLS Web Server Authentication");
    if (dottedOid == QLatin1StringView("1.3.6.1.5.5.7.3.2"))
        return QStringLiteral("TLS Web Client Authentication");
    if (dottedOid == QLatin1StringView("1.3.6.1.5.5.7.3.3"))
        return QStringLiteral("Code Signing");
    if (dottedOid == QLatin1StringView("1.3.6.1.5.5.7.3.4"))
        return QStringLiteral("E-mail Protection");
    if (dottedOid == QLatin1StringView("1.3.6.1.5.5.7.3.8"))
        return QStringLiteral("Time Stamping");
    if (dottedOid == QLatin1StringView("1.3.6.1.5.5.7.3.9"))
        return QStringLiteral("OCSP Signing");
    return dottedOid;
}

} // namespace librecelik::certformat
