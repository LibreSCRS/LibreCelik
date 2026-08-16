// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/collapsiblesection.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QString>

#include <map>
#include <set>

namespace librecelik::utils {

class FieldSectionBuilder
{
public:
    // Build a teal CollapsibleSection from a field group.
    // translationMap: field key -> translated label string.
    // Label precedence: translationMap, then the agent's own English label,
    // then the raw field key.
    // Empty-value fields are skipped; binary fields never reach the grid (the
    // shared client-side flatten rule drops them, so this renderer and the
    // other hosts' row renderers cannot drift).
    // hiddenFields: field keys to hide (e.g., foreigner-only fields when not foreigner).
    static CollapsibleSection* build(const QString& title, const LibreSCRS::AgentClient::FieldGroup& group,
                                     const std::map<QString, QString>& translationMap,
                                     const std::set<QString>& hiddenFields = {}, QWidget* parent = nullptr);

    // Post-process: highlight fields whose date (dd.MM.yyyy) is in the past.
    static void highlightExpiredDates(CollapsibleSection* section, const LibreSCRS::AgentClient::FieldGroup& group,
                                      const std::set<QString>& dateFieldKeys);
};

} // namespace librecelik::utils
