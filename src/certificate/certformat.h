// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

#include <QByteArrayView>
#include <QString>
#include <QtTypes>

class QDateTime;

/// @file
/// @brief Presentation helpers for certificate data the agent already parsed.
///
/// Nothing here parses X.509: the agent owns every ASN.1 decision and hands
/// this process display-ready strings plus two numeric members (the KeyUsage
/// bitmask and the validity instants). What survives here is the localized
/// label brain — the per-bit KeyUsage copy this application has always shown —
/// plus the byte/date renderings the viewer applies to values it receives raw.
namespace librecelik::certformat {

/// Colon-separated upper-case hex (e.g. `1A:2B:3C`) for byte blocks a caller
/// holds as raw data rather than as an agent-rendered string.
[[nodiscard]] QString bytesToHex(QByteArrayView bytes);

/// 16-bytes-per-line space-separated upper-case hex (e.g.
/// `30 82 03 4F …\n30 82 02 37 …`) for the forensic DER dump in the
/// unparseable-certificate path. `bytesPerLine` defaults to 16.
[[nodiscard]] QString bytesToHexLines(QByteArrayView bytes, qsizetype bytesPerLine = 16);

/// UTC long date+time format used for certificate validity periods in the
/// details view (`yyyy-MM-dd HH:mm:ss 'UTC'`). Empty for an invalid instant.
[[nodiscard]] QString formatTime(const QDateTime& tp);

/// Compact date used in the token section (`dd.MM.yyyy`, UTC). Empty for an
/// invalid instant.
[[nodiscard]] QString formatDate(const QDateTime& tp);

/// Localised single-bit label for a KeyUsage bit, via qtTrId.
/// @param bitIndex RFC 5280 §4.2.1.3 ordinal — the bit index within
///        `CertificateInfo::keyUsageBits` (digitalSignature = 0,
///        nonRepudiation = 1, …). Empty for an ordinal this build has no
///        label for; the mask is forward-compatible and a newer producer may
///        set a bit RFC 5280 does not define today.
[[nodiscard]] QString keyUsageBitLabel(int bitIndex);

/// Comma-separated localised KeyUsage rendering used by viewer / properties
/// (every labelled bit is rendered, in ascending ordinal order).
[[nodiscard]] QString keyUsageToString(quint32 keyUsageBits);

/// Comma-separated localised KeyUsage rendering used by the token section
/// summary (CA / EncipherOnly / DecipherOnly bits intentionally suppressed
/// — that section focuses on end-entity capability).
[[nodiscard]] QString keyUsageToStringEndEntity(quint32 keyUsageBits);

/// Human label for one ExtendedKeyUsage OID in dotted-decimal form.
///
/// FALLBACK PATH ONLY. The agent resolves EKU names against its own OID
/// database and ships them in the certificate's `eku` field group; a viewer
/// renders that group whenever it is present. This function serves the one
/// case the group cannot: an older agent that carries only the dotted OIDs on
/// `CertificateInfo::extendedKeyUsageOids`. It therefore knows nothing beyond
/// the handful of purposes RFC 5280 §4.2.1.12 itself names, and must never
/// grow into an OID database — that brain lives agent-side, on purpose, so
/// one process owns it and every client benefits from a single update.
/// Unknown input is returned unchanged.
[[nodiscard]] QString extendedKeyUsageLabel(const QString& dottedOid);

} // namespace librecelik::certformat
