// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QList>
#include <QString>
#include <QStringView>
#include <QVariant>

#include <algorithm>
#include <initializer_list>

/// @file
/// @brief Field lookups over the agent's field-group model — the successor of
///        the middleware-typed accessors LC widgets used to reach for.
///
/// Header-only and free of any state: a lookup helper that outlived its own
/// translation unit would only add a link edge for four loops. The keys are
/// taken as @c QStringView so a call site can pass a `u"..."` literal without
/// minting a QString per lookup.

namespace librecelik::plugin {
using LibreSCRS::AgentClient::Field;
using LibreSCRS::AgentClient::FieldGroup;

/// @brief Locate a group by its wire key; @c nullptr when absent.
///
/// Returns a pointer INTO @p groups — valid only as long as @p groups is
/// alive and unmodified. Call sites that hold a group across a re-read take a
/// copy instead.
[[nodiscard]] inline const FieldGroup* findGroup(const QList<FieldGroup>& groups, QStringView key)
{
    for (const FieldGroup& group : groups) {
        if (group.key == key) {
            return &group;
        }
    }
    return nullptr;
}

/// @brief Text value of @p fieldKey within an already-located group; empty
///        when the field is absent or carries no text (binary payloads ride
///        @ref fieldDetailBytes, never this).
[[nodiscard]] inline QString fieldValue(const FieldGroup& group, QStringView fieldKey)
{
    for (const Field& field : group.fields) {
        if (field.key == fieldKey) {
            return field.value;
        }
    }
    return {};
}

/// @brief Text value of @p fieldKey inside the group named @p groupKey; empty
///        when either is absent.
[[nodiscard]] inline QString fieldValue(const QList<FieldGroup>& groups, QStringView groupKey, QStringView fieldKey)
{
    const FieldGroup* group = findGroup(groups, groupKey);
    return group ? fieldValue(*group, fieldKey) : QString{};
}

/// @brief Flat lookup by field key across every group, first match wins
///        (groups in agent-supplied order).
///
/// The print/text-document formatters are the dominant caller: they query by
/// field key alone and have no group key on hand.
[[nodiscard]] inline QString fieldValue(const QList<FieldGroup>& groups, QStringView fieldKey)
{
    for (const FieldGroup& group : groups) {
        for (const Field& field : group.fields) {
            if (field.key == fieldKey) {
                return field.value;
            }
        }
    }
    return {};
}

/// @brief Raw bytes carried alongside a field — the merged photo being the one
///        the GUI asks for. Empty when the group, the field, or the payload is
///        absent.
[[nodiscard]] inline QByteArray fieldDetailBytes(const QList<FieldGroup>& groups, QStringView groupKey,
                                                 QStringView fieldKey)
{
    const FieldGroup* group = findGroup(groups, groupKey);
    if (!group) {
        return {};
    }
    for (const Field& field : group->fields) {
        if (field.key == fieldKey) {
            return field.detail.toByteArray();
        }
    }
    return {};
}

/// @brief The full wire model re-staged into a widget's own section order.
///
/// The final model's group order is DELIVERY-DEPENDENT — a streamed read
/// hands over stream order, a recovered (instant) read the wire map's keyed
/// order — while every streaming widget lays its sections out in ARRIVAL
/// order (and some chain: a group may build the container a later one hangs
/// off). A full-model ctor therefore stages the model into the widget's own
/// addGroup branch order first; arrival order is a wire artifact, not a
/// layout contract. Keys outside @p stageOrder keep their relative order
/// AFTER every staged one — data is preserved, never promoted ahead of a
/// designed section.
[[nodiscard]] inline QList<LibreSCRS::AgentClient::FieldGroup>
stagedForBuild(QList<LibreSCRS::AgentClient::FieldGroup> groups, std::initializer_list<QStringView> stageOrder)
{
    const auto stage = [&stageOrder](const LibreSCRS::AgentClient::FieldGroup& group) -> int {
        int rank = 0;
        for (const QStringView key : stageOrder) {
            if (group.key == key) {
                return rank;
            }
            ++rank;
        }
        return rank; // unknown keys: after every staged one, relative order kept
    };
    std::stable_sort(groups.begin(), groups.end(),
                     [&stage](const LibreSCRS::AgentClient::FieldGroup& a,
                              const LibreSCRS::AgentClient::FieldGroup& b) { return stage(a) < stage(b); });
    return groups;
}

} // namespace librecelik::plugin
