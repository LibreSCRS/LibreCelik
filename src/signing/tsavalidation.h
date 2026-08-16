// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>
#include <QStringView>
#include <QUrl>

namespace signing {

/// @brief Returns true if @p url is an acceptable TSA endpoint: well-formed
///        under QUrl::StrictMode, scheme is https, host is non-empty.
///
/// Rejects `http://` (no transport integrity on the token exchange),
/// malformed URLs (`https:/typo.com`), and empty input. Used by
/// FileSelectionPage and SignPage as defensive validation before signing.
///
/// Lives in its own header — and not in certutils — because it is the one
/// certutils entry point that never touched a certificate: the rest of that
/// header parses DER in-process, which the certificate display path no longer
/// does. Two implementations of a security predicate must never coexist, so
/// this is its single home.
[[nodiscard]] inline bool isValidTsaUrl(QStringView url)
{
    if (url.isEmpty())
        return false;
    const QUrl parsed(url.toString(), QUrl::StrictMode);
    return parsed.isValid() && parsed.scheme() == QStringLiteral("https") && !parsed.host().isEmpty();
}

} // namespace signing
