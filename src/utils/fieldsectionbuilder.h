// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include "utils/collapsiblesection.h"
#include <plugin/card_data.h>
#include <QString>
#include <map>
#include <set>
#include <string>

namespace LibreSCRS {

class FieldSectionBuilder
{
public:
    // Build a teal CollapsibleSection from a CardFieldGroup.
    // translationMap: field.key -> translated label string.
    // If key not in map, field.key is used as label (fallback).
    // Empty-value fields are skipped.
    // hiddenFields: field keys to hide (e.g., foreigner-only fields when not foreigner).
    static CollapsibleSection* build(const QString& title, const plugin::CardFieldGroup& group,
                                     const std::map<std::string, QString>& translationMap,
                                     const std::set<std::string>& hiddenFields = {}, QWidget* parent = nullptr);
};

} // namespace LibreSCRS
