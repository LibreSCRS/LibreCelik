// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "settings/settingskeys.h"
#include "signing/signingconfiguration.h"

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSettings>
#include <QStandardPaths>

namespace {
struct SettingsFixture : public ::testing::Test
{
    void SetUp() override
    {
        QCoreApplication::setOrganizationName(settings::kOrganization);
        QCoreApplication::setApplicationName(settings::kApplication);
        QSettings s;
        s.clear();
    }
};
} // namespace

TEST_F(SettingsFixture, TrustConfigFallsBackToDefaultsWhenUnset)
{
    signing::SigningConfiguration cfg;
    auto trust = cfg.makeTrustConfig();
    ASSERT_FALSE(trust.trustedListSources.empty());
    EXPECT_EQ(trust.trustedListSources.front().url, "https://www.mit.gov.rs/TrustedList/TSL-RS.xml");
    EXPECT_TRUE(trust.cacheDirectory.has_value());
}

TEST_F(SettingsFixture, TrustConfigReadsCustomEntries)
{
    QJsonArray arr;
    QJsonObject entry;
    entry["url"] = "https://example.invalid/tsl.xml";
    entry["lotl"] = true;
    entry["eager"] = false;
    arr.append(entry);
    QSettings s;
    s.setValue(settings::kTslEntries, QJsonDocument(arr).toJson(QJsonDocument::Compact));

    signing::SigningConfiguration cfg;
    auto trust = cfg.makeTrustConfig();
    ASSERT_EQ(trust.trustedListSources.size(), 1u);
    EXPECT_EQ(trust.trustedListSources.front().url, "https://example.invalid/tsl.xml");
    EXPECT_TRUE(trust.trustedListSources.front().lotl);
    EXPECT_FALSE(trust.trustedListSources.front().eager);
}

TEST_F(SettingsFixture, TsaProviderFallsBackToDefaultWhenUnset)
{
    signing::SigningConfiguration cfg;
    auto provider = cfg.makeTsaProvider();
    ASSERT_TRUE(static_cast<bool>(provider));

    LibreSCRS::Signing::TsaContext ctx;
    auto out = provider(ctx);
    // With no QSettings value, the dynamic provider must fall back to the
    // first entry in signing::defaultTsaUrls() so B-T+ signatures still
    // pick up a sane default on a freshly-provisioned install.
    EXPECT_EQ(out.url, "https://timestamp.sectigo.com");
}

TEST_F(SettingsFixture, TsaProviderFromSettings)
{
    QSettings s;
    s.setValue(QStringLiteral("signing/tsaUrl"), QStringLiteral("https://ts.example.invalid"));

    signing::SigningConfiguration cfg;
    auto provider = cfg.makeTsaProvider();
    ASSERT_TRUE(static_cast<bool>(provider));

    LibreSCRS::Signing::TsaContext ctx;
    auto out = provider(ctx);
    EXPECT_EQ(out.url, "https://ts.example.invalid");
}
