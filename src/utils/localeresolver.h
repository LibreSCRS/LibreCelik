// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QString>
#include <QStringList>

namespace utils {

/// @brief Locale codes for which LC ships translation .qm resources.
///
/// Single source of truth for both the application init path
/// (LibreCelik ctor) and the Settings dialog language dropdown
/// population. Keep this list in sync with the .qm resources compiled
/// into the binary.
[[nodiscard]] QStringList supportedLocaleCodes();

/// @brief Resolve the locale code that LC's UI should render in.
///
/// Pure function; deterministic from inputs. Used by both the
/// application init path and the Settings dialog dropdown init so they
/// agree on what "active language" means.
///
/// Resolution order:
///  1. @p userPreference if non-empty AND in @p supportedCodes — the
///     user's explicit choice from a previous Settings save.
///  2. else: walk @p systemUiLanguages (the platform's UI-language
///     preference list, ordered most-preferred-first); return the
///     first whose `QLocale::name()` is in @p supportedCodes.
///  3. else: `"en"` if in @p supportedCodes — last-resort default.
///  4. else: `supportedCodes.value(0)` — pathological fallback (caller
///     contract: list never empty).
///
/// @par Why this exists
/// Without this, the LibreCelik ctor's inline fallback chain and the
/// Settings dialog dropdown's `findData(kLanguage)` could diverge —
/// the app loads Serbian via system fallback, but Settings reads an
/// empty `kLanguage` and the dropdown stays at index 0 = English. The
/// UI is mismatched against the user's view of "what language am I
/// in?" Reported in 4.0.0-rc1.
[[nodiscard]] QString resolveActiveLocale(const QString& userPreference, const QStringList& supportedCodes,
                                          const QStringList& systemUiLanguages);

} // namespace utils
