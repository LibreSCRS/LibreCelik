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

// Signing — this application's own preference, held locally.
inline constexpr QLatin1String kSigningDefaultOutputFolder{"signing/defaultOutputFolder"};

// The 4.2 signing and trust preferences. The agent owns these values now, and
// nothing here reads their contents or writes them: they exist only so the
// Settings dialog can tell whether this profile still carries a 4.2
// configuration and should be told once that it is no longer imported.
inline constexpr QLatin1String kSigningDefaultLevel{"signing/defaultLevel"};
inline constexpr QLatin1String kSigningTsaUrls{"signing/tsaUrls"};
inline constexpr QLatin1String kSigningReason{"signing/reason"};
inline constexpr QLatin1String kSigningLocation{"signing/location"};
inline constexpr QLatin1String kTslEntries{"tsl/entries"};

// One-time marker for that notice. It is a NEW key rather than the import
// marker it replaces: a profile that saw 4.2's "you can apply these under
// Settings" line was told the opposite of what this one says, and reusing the
// old marker would silence the correction for exactly the people who need it.
inline constexpr QLatin1String kLegacyImportDroppedNoticeShown{"migration/legacyImportDroppedNoticeShown"};

} // namespace settings
