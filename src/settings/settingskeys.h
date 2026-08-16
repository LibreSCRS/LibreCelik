// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QLatin1String>

namespace settings {

// Organization and application names for QSettings construction
inline constexpr QLatin1String kOrganization{"LibreSCRS"};
inline constexpr QLatin1String kApplication{"LibreCelik"};

// General
inline constexpr QLatin1String kLanguage{"language"};

// Signing
inline constexpr QLatin1String kSigningDefaultLevel{"signing/defaultLevel"};
inline constexpr QLatin1String kSigningDefaultOutputFolder{"signing/defaultOutputFolder"};
inline constexpr QLatin1String kSigningTsaUrls{"signing/tsaUrls"};
inline constexpr QLatin1String kSigningTsaLastUrl{"signing/tsaLastUrl"};
inline constexpr QLatin1String kSigningReason{"signing/reason"};
inline constexpr QLatin1String kSigningLocation{"signing/location"};

// Trust / TSL
inline constexpr QLatin1String kTslEntries{"tsl/entries"};
inline constexpr QLatin1String kTslCacheDir{"tsl/cacheDir"};

// Migration markers for the legacy-QSettings → Config1 import. PER ITEM, not
// per run: a single "done" flag would either re-run every key after one
// transport failure or abandon them all after one refusal, and the agent's
// configuration is shared — an item that landed must never be written twice.
// The prefix is completed with the WIRE key ("…/DefaultLevel"); 1 = imported,
// -1 = attempted and semantically refused (terminal, never retried).
inline constexpr QLatin1String kConfig1ImportPrefix{"migration/config1Import/"};
// One-time marker for the passive trust-tier notice. The trust keys are
// polkit-guarded, so they are never auto-written — this records only that the
// human has been told once where to apply them.
inline constexpr QLatin1String kConfig1TrustNoticeShown{"migration/config1TrustNoticeShown"};

} // namespace settings
