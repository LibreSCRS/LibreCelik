// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QList>
#include <QString>
#include <QVariant>

class QSettings;

/// @file
/// @brief TIER-SPLIT import of the legacy QSettings preferences onto Config1.
///
/// The Config1 keys live in two polkit tiers (LibreLinux policy, pinned by
/// Config1Test):
///   - settings tier (`org.librescrs.agent.configure`, `allow_active=yes`):
///     DefaultLevel / DefaultReason / DefaultLocation — silent writes.
///   - trust tier (`org.librescrs.agent.configure.trust`, `auth_self` on EVERY
///     row): TsaUrls / TslSources — every write raises a polkit auth dialog.
///
/// Auto-importing trust keys at first Ready would fire an out-of-context polkit
/// prompt at STARTUP, and any refusal-retry loop would re-fire it at every
/// Ready (launch, agent restart, availability blip) — training click-through
/// and nagging decliners. Therefore: settings tier auto-imports; trust tier is
/// NEVER auto-written — it reaches the agent only through a user-clicked Save
/// in the Settings dialog, where the polkit ceremony is in context and
/// NotAuthorized is done-with-decline.

namespace librecelik::agent {

class AgentGateway;

/// One key/value the legacy configuration has to offer, in the WIRE's spelling.
struct ImportItem
{
    QString wireKey;
    QVariant value;
};

struct ImportPlan
{
    QList<ImportItem> settingsTier; ///< DefaultLevel/DefaultReason/DefaultLocation — auto-import
    QList<ImportItem> trustTier;    ///< TsaUrls/TslSources — prefill + notice ONLY, never auto-written
};

/// Pure mapping of the legacy QSettings snapshot onto the two tiers.
/// - signing/defaultLevel "B_B|B_T|B_LT|B_LTA" → DefaultLevel "b-b|b-t|b-lt|b-lta"  (settings)
/// - signing/reason / signing/location        → DefaultReason / DefaultLocation      (settings)
/// - signing/tsaUrls  QStringList             → TsaUrls                              (trust)
/// - tsl/entries (JSON [{url,lotl,eager}])    → TslSources [[url,lotl,eager]…]       (trust)
///
/// Dropped (no wire home, recorded in the body): tsl/cacheDir (file-only),
/// signing/tsaLastUrl (LastTsaUrl is read-only agent state), legacy
/// signing/tsaUrl (dead key). Absent keys produce no item (agent defaults win).
[[nodiscard]] ImportPlan buildConfig1Import(QSettings& settings);

/// Auto-import of the SETTINGS tier only, with PER-ITEM completion markers
/// ("migration/config1Import/<WireKey>" = 1). Semantics per item:
///   - already marked → NEVER touched again (a succeeded import must not stomp
///     values the user has since changed — the KDE surfaces share this agent);
///   - write succeeds → mark;
///   - named semantic refusal (UnknownConfigKey/ReadOnlyConfig/
///     InvalidConfigValue/NotAuthorized) → mark as attempted (value "-1"),
///     log once — a permanently-malformed legacy value must not re-run forever;
///   - transport-shaped failure (agent vanished mid-call) → leave UNMARKED; the
///     next Ready retries just that item.
/// Whole run SKIPPED (no writes, no markers) when settingsTier is empty.
void runSettingsTierImport(AgentGateway& gateway, QSettings& settings);

/// True exactly once: legacy trust-tier values exist, the one-time notice
/// ("migration/config1TrustNoticeShown") has not been shown yet. The caller
/// shows a PASSIVE statusbar line (`lc-settings-trust-import-notice`) — no
/// dialog, no write, no polkit.
[[nodiscard]] bool shouldShowTrustImportNotice(QSettings& settings);

} // namespace librecelik::agent
