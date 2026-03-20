// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CARDDATAUTILS_H
#define CARDDATAUTILS_H

#include <plugin/card_data.h>
#include <QString>

namespace plugin {

inline QString getFieldValue(const CardFieldGroup* group, const std::string& key)
{
    if (!group)
        return {};
    for (const auto& field : group->fields) {
        if (field.key == key)
            return QString::fromStdString(field.asString());
    }
    return {};
}

} // namespace plugin

#endif // CARDDATAUTILS_H
