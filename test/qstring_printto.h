// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Readable QString diagnostics for GoogleTest failures.
///
/// Without this, a QString mismatch renders as pages of raw UTF-16 byte
/// objects, which is precisely useless when the assertions are about which
/// WORDS a user ends up reading. Shared by every AgentLayerTests translation
/// unit that compares QStrings, so there is exactly one definition rather
/// than one per file that a future edit could silently drift out of step
/// with — GoogleTest's `PrintTo` overload has to be visible in each
/// translation unit that uses it, so it cannot simply live once in a .cpp.

#pragma once

#include <QString>

#include <ostream>

inline void PrintTo(const QString& value, std::ostream* os)
{
    *os << '"' << value.toStdString() << '"';
}
