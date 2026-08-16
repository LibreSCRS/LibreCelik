// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/settingsimport.h"

#include "agent/agentgateway.h"
#include "settings/settingskeys.h"

#include <LibreSCRS/AgentClient/SyncError.h>

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSettings>
#include <QStringList>
#include <QUrl>
#include <QVariantList>

namespace {

using LibreSCRS::AgentClient::SyncError;

// Config1 keys, in the wire's own spelling — the names the agent answers to,
// not the QSettings paths the same preferences used to live under.
constexpr QLatin1String kDefaultLevel{"DefaultLevel"};
constexpr QLatin1String kDefaultReason{"DefaultReason"};
constexpr QLatin1String kDefaultLocation{"DefaultLocation"};
constexpr QLatin1String kTsaUrls{"TsaUrls"};
constexpr QLatin1String kTslSources{"TslSources"};

/// Same shape the dialog re-validates on read with: the legacy store is a plain
/// file a human can edit, and a value that was never a service URL has no
/// business being carried into the agent's configuration — or, for the trust
/// tier, into a notice that points at nothing.
bool isValidServiceUrl(const QString& url)
{
    QUrl parsed(url);
    if (!parsed.isValid() || parsed.host().isEmpty())
        return false;
    const QString scheme = parsed.scheme();
    return scheme == QStringLiteral("https") || scheme == QStringLiteral("http");
}

/// The legacy level tokens, mapped onto the wire's. Deliberately CLOSED: a
/// token outside the four has no mapping, and an unmapped legacy value must
/// produce no item rather than a silent write of the baseline level.
QString wireLevelToken(const QString& legacyToken)
{
    if (legacyToken == QStringLiteral("B_B"))
        return QStringLiteral("b-b");
    if (legacyToken == QStringLiteral("B_T"))
        return QStringLiteral("b-t");
    if (legacyToken == QStringLiteral("B_LT"))
        return QStringLiteral("b-lt");
    if (legacyToken == QStringLiteral("B_LTA"))
        return QStringLiteral("b-lta");
    return QString();
}

/// The semantic-vs-transport partition, in one place.
///
/// Terminal = the agent understood the request and said no for a reason a retry
/// cannot change. Retryable = everything else, which in practice is
/// `CommunicationError`: the client maps every the-write-never-arrived failure
/// onto it. It is ALSO where an unrecognised wire name lands (the decode
/// warning on `SyncError`), so an exotic future refusal retries once per Ready
/// — a silent settings-tier write, never a prompt. Acceptable, and noted.
bool isTerminalRefusal(SyncError error)
{
    switch (error) {
    case SyncError::UnknownConfigKey:
    case SyncError::ReadOnlyConfig:
    case SyncError::InvalidConfigValue:
    case SyncError::NotAuthorized:
        return true;
    default:
        return false;
    }
}

QString markerKey(const QString& wireKey)
{
    return QString(settings::kConfig1ImportPrefix) + wireKey;
}

/// The legacy trusted-list store: a compact JSON array, with a fallback to the
/// even older QVariant-held array the first releases wrote. Both spellings are
/// read because both exist in the wild.
QJsonArray legacyTslEntries(QSettings& settings)
{
    const QVariant raw = settings.value(settings::kTslEntries);
    QJsonArray entries = QJsonDocument::fromJson(raw.toByteArray()).array();
    if (entries.isEmpty())
        entries = raw.toJsonArray();
    return entries;
}

} // namespace

namespace librecelik::agent {

ImportPlan buildConfig1Import(QSettings& settings)
{
    ImportPlan plan;

    // --- settings tier -------------------------------------------------------
    if (settings.contains(settings::kSigningDefaultLevel)) {
        const QString level = wireLevelToken(settings.value(settings::kSigningDefaultLevel).toString().trimmed());
        if (!level.isEmpty())
            plan.settingsTier.append({QString(kDefaultLevel), QVariant(level)});
    }
    if (settings.contains(settings::kSigningReason)) {
        const QString reason = settings.value(settings::kSigningReason).toString();
        if (!reason.isEmpty())
            plan.settingsTier.append({QString(kDefaultReason), QVariant(reason)});
    }
    if (settings.contains(settings::kSigningLocation)) {
        const QString location = settings.value(settings::kSigningLocation).toString();
        if (!location.isEmpty())
            plan.settingsTier.append({QString(kDefaultLocation), QVariant(location)});
    }

    // --- trust tier ----------------------------------------------------------
    if (settings.contains(settings::kSigningTsaUrls)) {
        QStringList urls;
        for (const QString& rawUrl : settings.value(settings::kSigningTsaUrls).toStringList()) {
            const QString url = rawUrl.trimmed();
            if (!url.isEmpty() && isValidServiceUrl(url))
                urls.append(url);
        }
        if (!urls.isEmpty())
            plan.trustTier.append({QString(kTsaUrls), QVariant(urls)});
    }
    if (settings.contains(settings::kTslEntries)) {
        QVariantList sources;
        const QJsonArray entries = legacyTslEntries(settings);
        for (const auto& entry : entries) {
            const QJsonObject obj = entry.toObject();
            const QString url = obj.value(QStringLiteral("url")).toString().trimmed();
            if (url.isEmpty() || !isValidServiceUrl(url))
                continue;
            // [url, lotl, eager] — the shape both transports normalise to. The
            // legacy defaults are the ones the old dialog read with.
            sources.append(QVariant(QVariantList{url, obj.value(QStringLiteral("lotl")).toBool(false),
                                                 obj.value(QStringLiteral("eager")).toBool(true)}));
        }
        if (!sources.isEmpty())
            plan.trustTier.append({QString(kTslSources), QVariant(sources)});
    }

    // Deliberately dropped, and this is the record of why:
    //   - tsl/cacheDir       file-only; the agent owns its own cache location
    //                        and exports no key for it.
    //   - signing/tsaLastUrl LastTsaUrl is READ-ONLY agent state — what the
    //                        agent last used, not a preference to seed.
    //   - signing/tsaUrl     dead key from before the list-valued one.

    return plan;
}

void runSettingsTierImport(AgentGateway& gateway, QSettings& settings)
{
    const ImportPlan plan = buildConfig1Import(settings);
    if (plan.settingsTier.isEmpty())
        return; // nothing to import: no writes, and no markers to spend

    // The trust tier is NOT iterated here, and that is the whole point of the
    // split — see the header. It reaches the agent through a user-clicked Save
    // or not at all.
    for (const ImportItem& item : plan.settingsTier) {
        const QString marker = markerKey(item.wireKey);
        if (settings.contains(marker))
            continue; // done once, ever — a later user edit is not ours to stomp

        const auto refusal = gateway.setConfigValue(item.wireKey, item.value);
        if (!refusal.has_value()) {
            settings.setValue(marker, 1);
            continue;
        }
        if (isTerminalRefusal(*refusal)) {
            // Said once, then never again: a permanently-malformed legacy value
            // must not re-run at every Ready for the life of the installation.
            qWarning() << "settings import: the agent refused" << item.wireKey
                       << "- recording the attempt and moving on";
            settings.setValue(marker, -1);
            continue;
        }
        // Transport-shaped: the write never arrived. Leave the item unmarked so
        // the next Ready retries exactly this one.
    }
}

bool shouldShowTrustImportNotice(QSettings& settings)
{
    if (settings.contains(settings::kConfig1TrustNoticeShown))
        return false;
    return !buildConfig1Import(settings).trustTier.isEmpty();
}

} // namespace librecelik::agent
