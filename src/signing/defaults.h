// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QStringList>

namespace signing {

inline const QStringList& defaultTsaUrls()
{
    static const QStringList urls = {
        QStringLiteral("https://timestamp.sectigo.com"),
        QStringLiteral("https://timestamp.digicert.com"),
        QStringLiteral("https://ts.ssl.com"),
    };
    return urls;
}

} // namespace signing
