// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/Plugin/CardData.h>
#include <LibreSCRS/Plugin/CardDataAccess.h>
#include <QString>

#include <cstddef>
#include <optional>
#include <string>
#include <string_view>

namespace librecelik::plugin {

using ::LibreSCRS::Plugin::CardData;
using ::LibreSCRS::Plugin::CardFieldGroup;

/// @brief Look up a field value by key within a group, returning its text
/// representation (empty QString if the group is missing, the field is
/// absent, or the field is non-textual).
///
/// @note LM has no equivalent — the Wave 6 accessors operate on @ref CardData,
/// not on a pre-fetched @ref CardFieldGroup pointer. This overload remains
/// hand-rolled because most LC widget call sites already hold a group
/// pointer (cached from an earlier @ref CardData::findGroup) and would
/// otherwise pay for a redundant second lookup.
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
/// group and then the field. Delegates to @ref LibreSCRS::Plugin::textValue
/// (LM 4.2 Wave 6); this wrapper exists only to adapt the LM
/// `std::optional<std::string>` return to LC's QString convention.
inline QString getFieldValue(const CardData& data, const std::string& groupKey, const std::string& fieldKey)
{
    if (auto value = ::LibreSCRS::Plugin::textValue(data, std::string_view{groupKey}, std::string_view{fieldKey}))
        return QString::fromStdString(*value);
    return {};
}

/// @brief Overload accepting the optional-index returned by
/// @ref CardData::findGroup paired with the owning @ref CardData (needed to
/// dereference the index). Delegates to @ref LibreSCRS::Plugin::textValueAt
/// (LM 4.2 Wave 6) when the index is engaged.
///
/// @ref CardData::findGroup returns `std::optional<std::size_t>` — a raw
/// index that is stable against pimpl moves and encodes the lifetime
/// contract explicitly. Pair it with the owning @ref CardData via this
/// overload as `getFieldValue(data, data.findGroup(...), key)`.
inline QString getFieldValue(const CardData& data, std::optional<std::size_t> groupIndex, const std::string& fieldKey)
{
    if (!groupIndex)
        return {};
    if (auto value = ::LibreSCRS::Plugin::textValueAt(data, *groupIndex, std::string_view{fieldKey}))
        return QString::fromStdString(*value);
    return {};
}

/// @brief Flat field-key lookup across all groups (first match wins).
///
/// @note LM has no equivalent flat accessor — @ref LibreSCRS::Plugin::textValue
/// requires both a group key and a field key, and @ref textValueAt requires
/// an explicit group index. This overload stays hand-rolled because LC
/// textdocument formatters (the dominant call site, e.g. emrtdtextdocument.cpp)
/// query by field key alone and do not have a group key on hand.
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
