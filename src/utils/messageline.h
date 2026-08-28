// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The one place that builds a centred, word-wrapped line of text for
///        a placeholder page, so it is also the one place that pins its
///        text format.
///
/// The read-in-progress page, the unreadable-card page and the recoverable
/// read-failure page each stack a QVBoxLayout of these lines between two
/// stretches. Before this, each built its own labels inline, and the two
/// pages that substitute card- or reader-supplied text had already learned
/// (independently, in near-identical comments) to pin `Qt::PlainText`: under
/// the default AutoText, a reader name or an ATR with angle brackets in it is
/// parsed as HTML and partly vanishes from the very line that exists to show
/// it. The third page, which only ever substitutes a fixed guidance string,
/// had not. Factoring the construction out means there is one guarantee
/// instead of one convention copied by hand into whichever page remembers to.

#pragma once

#include <QString>

class QLabel;
class QVBoxLayout;
class QWidget;

namespace librecelik::utils {

/// Appends a centred, word-wrapped, always-plain-text line to @p layout,
/// owned by @p parent.
///
/// @param objectName Set on the label when non-empty, so a test can find this
///                    exact line back by name.
/// @param bold        Whether the line renders as the page's headline.
[[nodiscard]] QLabel* addMessageLine(QVBoxLayout* layout, QWidget* parent, const QString& objectName = QString(),
                                     bool bold = false);

} // namespace librecelik::utils
