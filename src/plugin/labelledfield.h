// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>
#include <QStringView>

/// @file
/// @brief One wire field key paired with the label a document prints for it.
///
/// The print formatters each lay out a fixed table: a list of field keys in
/// reading order, each with its own translated caption. That pairing was
/// re-declared inside every function that built such a table. It is a model of
/// a row, not a lookup, so it lives here rather than beside the field
/// accessors.
///
/// Deliberately NOT called `Field`: that name is already taken in this
/// namespace by the agent's wire field, and a table row is not one of those.

namespace librecelik::plugin {

/// @brief A field key and the caption printed beside its value.
struct LabelledField
{
    QStringView key; ///< The wire field key to look up.
    QString label;   ///< The translated caption for the row.
};

} // namespace librecelik::plugin
