// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The read-in-progress placeholder page, and the test that tells it
///        apart from a built card page.
///
/// It lives here rather than inside the window that stacks it because its text
/// is GUIDANCE, not a caption: the pre-authentication line tells the holder
/// where to find the CAN or MRZ the agent is about to ask for, and guidance
/// that renders half-clipped is worse than none. A page whose layout must hold
/// an arbitrary number of wrapped lines is worth proving, and proving it needs
/// it reachable from a test.

#pragma once

#include <QString>

class QWidget;

namespace librecelik::utils {

/// The read-in-progress page. @p text names what is being waited for — a card
/// read, or the holder's answer in the agent's own dialog.
[[nodiscard]] QWidget* makeSpinnerPage(const QString& text, QWidget* parent);

/// Whether @p widget is the read-in-progress placeholder rather than a built
/// card page.
[[nodiscard]] bool isSpinner(QWidget* widget);

} // namespace librecelik::utils
