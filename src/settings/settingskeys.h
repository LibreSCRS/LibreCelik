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

} // namespace settings
