// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
#pragma once

#include <LibreSCRS/Certificate/ParsedCertificate.h>

#include <QString>

#include <chrono>
#include <cstdint>
#include <span>
#include <vector>

namespace librecelik::certformat {

/// Colon-separated upper-case hex (e.g. `1A:2B:3C`) for serial numbers, key
/// identifiers, signature values displayed inline.
[[nodiscard]] QString bytesToHex(std::span<const std::uint8_t> bytes);
[[nodiscard]] QString bytesToHex(const std::vector<std::uint8_t>& bytes);

/// 16-bytes-per-line space-separated upper-case hex (e.g.
/// `30 82 03 4F …\n30 82 02 37 …`) for forensic DER dumps in the
/// unparseable-cert path. `bytesPerLine` defaults to 16.
[[nodiscard]] QString bytesToHexLines(std::span<const std::uint8_t> bytes, std::size_t bytesPerLine = 16);

/// UTC long date+time format used for cert validity periods in the details
/// view (`yyyy-MM-dd HH:mm:ss 'UTC'`).
[[nodiscard]] QString formatTime(std::chrono::system_clock::time_point tp);

/// Compact local-style date used in the token section (`dd.MM.yyyy`).
[[nodiscard]] QString formatDate(std::chrono::system_clock::time_point tp);

/// Localised single-bit label for a KeyUsage bit, via qtTrId.
[[nodiscard]] QString keyUsageBitLabel(LibreSCRS::Certificate::KeyUsageBit bit);

/// Comma-separated localised KeyUsage rendering used by viewer / properties
/// (every bit is rendered).
[[nodiscard]] QString keyUsageToString(std::span<const LibreSCRS::Certificate::KeyUsageBit> bits);

/// Comma-separated localised KeyUsage rendering used by the token section
/// summary (CA / EncipherOnly / DecipherOnly bits intentionally suppressed
/// — that section focuses on end-entity capability).
[[nodiscard]] QString keyUsageToStringEndEntity(std::span<const LibreSCRS::Certificate::KeyUsageBit> bits);

/// Localised label for a GeneralName type tag (Email / DNS / URI / IP /
/// Directory / Registered ID / Other Name / X.400 / EDI).
[[nodiscard]] QString generalNameTypeLabel(LibreSCRS::Certificate::GeneralNameType type);

/// Localised public-key algorithm label, with curve OID appended in
/// parentheses when present (e.g. `"ECDSA (P-256)"`, falling back to the
/// dotted-decimal OID when the curve is not in the friendly-name DB).
/// Falls back to the parsed `algorithmDescription` when the algorithm is
/// `Other` and the description is non-empty, and to
/// `qtTrId("lc-cert-algorithm-unknown")` when even the description is empty.
[[nodiscard]] QString publicKeyAlgorithmLabel(const LibreSCRS::Certificate::PublicKeyInfo& pk);

/// Full one-line public-key description used by the token section:
/// `"<algorithm> <bitLength>-bit"` with curve appended in parentheses for
/// ECDSA when known (e.g. `"ECDSA 256-bit (P-256)"`).
[[nodiscard]] QString publicKeyDescription(const LibreSCRS::Certificate::PublicKeyInfo& pk);

} // namespace librecelik::certformat
