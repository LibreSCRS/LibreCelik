// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <QString>
#include <QStringList>

inline QString cleanAddress(const QString& raw)
{
    if (raw.isEmpty())
        return {};

    QStringList parts;
    for (const auto& segment : raw.split(',')) {
        auto trimmed = segment.trimmed();
        if (!trimmed.isEmpty())
            parts.append(trimmed);
    }
    return parts.join(", ");
}
