// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "utils/securitystatuswidget.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QLatin1StringView>
#include <QString>

/// @file
/// @brief The one reader of the `security_status` field group.
///
/// A card read carries its verification result as flat wire fields: three
/// `overall_*` verdicts, and every individual check spread over
/// `check_<N>_<suffix>` with suffixes `id`, `category`, `status`, `label`,
/// `detail`, `error` and `reason`. Two surfaces render that group — the pane
/// on screen and the printed record — and a second parser is precisely how
/// the two would come to disagree about which index a label belongs to, or
/// what an unnamed status means. Header-only and stateless, so a surface pays
/// for the shape and not for a link edge.

namespace librecelik::utils {

/// @brief Parse a `security_status` group into the model both surfaces render.
///
/// Foreign input throughout: an unrecognised status or category is not an
/// error but a token this build has not learned, and collapsing it to the
/// safest-LOOKING value is worse than admitting the check was not performed.
/// An index the read never filled leaves a default-constructed entry in place
/// — a check with no id and no label — rather than shifting every later check
/// up one and mislabelling all of them.
[[nodiscard]] inline SecurityStatusModel securityStatusFromGroup(const LibreSCRS::AgentClient::FieldGroup& group)
{
    constexpr qsizetype kCheckPrefixLength = 6; // "check_"

    SecurityStatusModel model;
    for (const auto& field : group.fields) {
        const QString& text = field.value;
        if (field.key == QLatin1StringView("overall_integrity")) {
            model.overallIntegrity = statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
        } else if (field.key == QLatin1StringView("overall_authenticity")) {
            model.overallAuthenticity = statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
        } else if (field.key == QLatin1StringView("overall_genuineness")) {
            model.overallGenuineness = statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
        } else if (field.key.startsWith(QLatin1StringView("check_"))) {
            const qsizetype separator = field.key.indexOf(u'_', kCheckPrefixLength);
            if (separator < 0) {
                continue;
            }
            const QString suffix = field.key.mid(separator + 1);
            const QString idxStr = field.key.mid(kCheckPrefixLength, separator - kCheckPrefixLength);
            bool parsed = false;
            const uint idx = idxStr.toUInt(&parsed);
            if (!parsed) {
                continue;
            }
            while (model.checks.size() <= static_cast<qsizetype>(idx)) {
                model.checks.emplaceBack();
            }
            auto& check = model.checks[static_cast<qsizetype>(idx)];
            if (suffix == QLatin1StringView("id")) {
                check.checkId = text;
            } else if (suffix == QLatin1StringView("category")) {
                check.category = categoryFromString(text).value_or(SecurityCategory::Other);
            } else if (suffix == QLatin1StringView("status")) {
                check.status = statusFromString(text).value_or(SecurityCheck::Status::NotPerformed);
            } else if (suffix == QLatin1StringView("label")) {
                check.label = text;
            } else if (suffix == QLatin1StringView("detail")) {
                check.detail = text;
            } else if (suffix == QLatin1StringView("error")) {
                check.errorDetail = text;
            } else if (suffix == QLatin1StringView("reason")) {
                check.reason = text;
            }
        }
    }
    return model;
}

} // namespace librecelik::utils
