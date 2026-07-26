// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>

namespace librecelik::signing {

/// @brief Render a PKCS#11 slot description for the wizard dropdown.
///
/// Combines the token label with a localised PIN-label
/// (Authentication / Signing / QSCD). Unknown raw PIN labels are
/// passed through verbatim so the formatter is robust against
/// cards whose AODF emits non-canonical strings.
///
/// Multi-PIN cards (e.g. some eID cards) expose one PKCS#11 slot
/// per PIN object. The wizard dropdown shows one entry per slot
/// using this formatter.
///
/// Examples:
/// @code
/// formatSlotLabel("Sample eID", "Authentication")  // "Sample eID — Authentication"
/// formatSlotLabel("Sample eID", "Signing (QSCD)")  // "Sample eID — Signing (QSCD)"
/// formatSlotLabel("",        "Authentication")     // "Authentication"
/// formatSlotLabel("Card",    "Custom-PIN")         // "Card — Custom-PIN"
/// @endcode
[[nodiscard]] QString formatSlotLabel(const QString& tokenLabel, const QString& rawPinLabel);

} // namespace librecelik::signing
