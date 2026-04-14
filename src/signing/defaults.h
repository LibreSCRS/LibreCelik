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

struct TlDefault
{
    QString url;
    bool lotl;
    bool eager;
};

inline const QList<TlDefault>& defaultTrustedLists()
{
    static const QList<TlDefault> lists = {
        {QStringLiteral("https://www.mit.gov.rs/TrustedList/TSL-RS.xml"), false, true},
        {QStringLiteral("https://ec.europa.eu/tools/lotl/eu-lotl.xml"), true, false},
    };
    return lists;
}

} // namespace signing
