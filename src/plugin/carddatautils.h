// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Plugin/CardData.h>
#include <QString>

#include <optional>
#include <utility>

namespace librecelik::plugin {

using ::LibreSCRS::Plugin::CardData;
using ::LibreSCRS::Plugin::CardFieldGroup;

/// @brief Look up a field value by key within a group, returning its text
/// representation (empty QString if the group is missing, the field is
/// absent, or the field is non-textual).
inline QString getFieldValue(const CardFieldGroup* group, const std::string& key)
{
    if (!group)
        return {};
    for (const auto& field : group->fields) {
        if (field.key == key) {
            if (auto text = field.textValue())
                return QString::fromStdString(*text);
            return {};
        }
    }
    return {};
}

/// @brief Overload accepting a @ref CardData + group key — looks up the
/// group and then the field, collapsing the two-step index dance the
/// `findGroup(optional<size_t>)` API requires.
inline QString getFieldValue(const CardData& data, const std::string& groupKey, const std::string& fieldKey)
{
    auto idx = data.findGroup(groupKey);
    if (!idx)
        return {};
    return getFieldValue(&data.groupAt(*idx), fieldKey);
}

/// @brief Overload accepting the optional-index returned by
/// @ref CardData::findGroup paired with the owning @ref CardData (needed to
/// dereference the index).
///
/// @ref CardData::findGroup returns `std::optional<std::size_t>` — a raw
/// index that is stable against pimpl moves and encodes the lifetime
/// contract explicitly. Pair it with the owning @ref CardData via this
/// overload as `getFieldValue(data, data.findGroup(...), key)`.
inline QString getFieldValue(const CardData& data, std::optional<std::size_t> groupIndex, const std::string& fieldKey)
{
    if (!groupIndex)
        return {};
    return getFieldValue(&data.groupAt(*groupIndex), fieldKey);
}

inline QString getFieldValue(const CardData& data, const std::string& key)
{
    auto idx = data.findField(key);
    if (!idx)
        return {};
    const auto& field = data.fieldAt(*idx);
    if (auto text = field.textValue())
        return QString::fromStdString(*text);
    return {};
}

} // namespace librecelik::plugin
