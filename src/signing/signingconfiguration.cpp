// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "signing/signingconfiguration.h"
#include "settings/settingskeys.h"
#include "signing/defaults.h"
#include "utils/libreceliklog.h"

#include <LibreSCRS/Signing/TsaProvider.h>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

#include <filesystem>

namespace signing {

SigningConfiguration::SigningConfiguration() = default;

LibreSCRS::Trust::TrustConfig SigningConfiguration::makeTrustConfig() const
{
    QSettings settings(settings::kOrganization, settings::kApplication);

    LibreSCRS::Trust::TrustConfig trust;

    auto tlEntries = QJsonDocument::fromJson(settings.value(settings::kTslEntries).toByteArray()).array();
    if (tlEntries.isEmpty())
        tlEntries = settings.value(settings::kTslEntries).toJsonArray();

    if (tlEntries.isEmpty()) {
        for (const auto& d : signing::defaultTrustedLists()) {
            trust.trustedListSources.push_back({d.url.toStdString(), d.lotl, d.eager});
        }
    } else {
        for (const auto& entry : tlEntries) {
            const auto obj = entry.toObject();
            trust.trustedListSources.push_back({
                obj["url"].toString().toStdString(),
                obj["lotl"].toBool(false),
                obj["eager"].toBool(true),
            });
        }
    }

    QString cacheDir = settings.value(settings::kTslCacheDir).toString();
    if (cacheDir.isEmpty()) {
        const QString xdg = QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation);
        cacheDir = xdg + QStringLiteral("/librescrs/tsl");
    }
    trust.cacheDirectory = std::filesystem::path(cacheDir.toStdString());

    return trust;
}

LibreSCRS::Signing::TsaProvider SigningConfiguration::makeTsaProvider() const
{
    // Return a dynamic provider rather than a staticTsa snapshot so that
    // the SettingsDialog's TSA URL changes take effect on the next sign()
    // call without rebuilding the SigningService. Per-wizard overrides
    // from FileSelectionPage are still applied on top by SignPage before
    // it starts signing (see SignPage::startSigning).
    return [](const LibreSCRS::Signing::TsaContext&) -> LibreSCRS::Signing::TsaRequest {
        QSettings settings(settings::kOrganization, settings::kApplication);
        QString url = settings.value(QStringLiteral("signing/tsaUrl")).toString();
        if (url.isEmpty()) {
            // Review D2: log when the fallback is taken so eventual TSA
            // failures have a diagnosable trail (no user-configured URL).
            const auto& defaults = signing::defaultTsaUrls();
            if (!defaults.isEmpty()) {
                url = defaults.first();
                qCDebug(lcGeneral) << "SigningConfiguration: no user TSA URL configured; "
                                      "falling back to bundled default"
                                   << url;
            }
        }
        LibreSCRS::Signing::TsaRequest req;
        req.url = url.toStdString();
        return req;
    };
}

} // namespace signing
