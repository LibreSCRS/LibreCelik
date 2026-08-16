// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Coverage for the legacy QSettings → Config1 import, tier by tier.
///
/// The interesting property here is not the vocabulary mapping (though that is
/// asserted too) — it is WHICH keys the import is allowed to write on its own.
/// The trust-tier keys are polkit `auth_self` on every row: auto-importing them
/// at first Ready would raise an authorisation dialog the human never asked
/// for, at the least explicable moment there is, and would raise it again at
/// every subsequent Ready. So the settings tier imports silently and the trust
/// tier never reaches the agent from here at all — it is prefilled into the
/// Settings dialog and announced by a passive notice, and the ceremony happens
/// on a user-clicked Save or not at all.
///
/// Nothing here dials anything: the gateway is the campaign's scripted fake,
/// and the QSettings are an isolated ini file in a temporary directory.

#include "agent/settingsimport.h"

#include "fake_gateway/fakeagentgateway.h"

#include <LibreSCRS/AgentClient/SyncError.h>

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QVariant>
#include <QVariantList>

#include <gtest/gtest.h>

namespace {

using librecelik::test::agent::FakeAgentGateway;

// Isolated ini-backed QSettings so the test never touches the user scope.
struct TempSettings
{
    QTemporaryDir dir;
    QSettings s{dir.filePath(QStringLiteral("t.ini")), QSettings::IniFormat};
};

} // namespace

TEST(SettingsImport, MapsVocabulariesAndSplitsTiers)
{
    TempSettings t;
    t.s.setValue(QStringLiteral("signing/defaultLevel"), QStringLiteral("B_LT"));
    t.s.setValue(QStringLiteral("tsl/entries"),
                 QByteArrayLiteral(R"([{"url":"https://x/tl.xml","lotl":false,"eager":true}])"));
    const auto plan = librecelik::agent::buildConfig1Import(t.s);
    ASSERT_EQ(plan.settingsTier.size(), 1);
    EXPECT_EQ(plan.settingsTier[0].wireKey, QStringLiteral("DefaultLevel"));
    EXPECT_EQ(plan.settingsTier[0].value.toString(), QStringLiteral("b-lt"));
    ASSERT_EQ(plan.trustTier.size(), 1);
    EXPECT_EQ(plan.trustTier[0].wireKey, QStringLiteral("TslSources"));
    const auto rows = plan.trustTier[0].value.toList();
    ASSERT_EQ(rows.size(), 1);
    EXPECT_EQ(rows[0].toList(), (QVariantList{QStringLiteral("https://x/tl.xml"), false, true}));
}

TEST(SettingsImport, TrustTierIsNeverAutoWritten)
{
    FakeAgentGateway gw;
    TempSettings t;
    t.s.setValue(QStringLiteral("signing/tsaUrls"), QStringList{QStringLiteral("https://tsa")});
    t.s.setValue(QStringLiteral("tsl/entries"),
                 QByteArrayLiteral(R"([{"url":"https://x","lotl":true,"eager":false}])"));
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_TRUE(gw.configWrites.isEmpty()); // trust keys reach the agent ONLY via a user-clicked Save
    EXPECT_TRUE(librecelik::agent::shouldShowTrustImportNotice(t.s)); // passive notice instead
}

/// The case above scripts NO settings-tier key, so the run is skipped whole and
/// the trust tier is spared by the skip rather than by the split. This one puts
/// a settings-tier key alongside the trust-tier ones, so the import genuinely
/// ITERATES — and the trust keys must still be absent from everything it sends
/// and from every marker it spends. Without this case, widening the loop to
/// carry the trust tier would leave the suite green, and a startup polkit
/// prompt is exactly what that regression looks like in a user's hands.
TEST(SettingsImport, TrustTierStaysUnwrittenEvenWhileTheSettingsTierRuns)
{
    FakeAgentGateway gw;
    TempSettings t;
    t.s.setValue(QStringLiteral("signing/reason"), QStringLiteral("Approval"));
    t.s.setValue(QStringLiteral("signing/tsaUrls"), QStringList{QStringLiteral("https://tsa")});
    t.s.setValue(QStringLiteral("tsl/entries"),
                 QByteArrayLiteral(R"([{"url":"https://x","lotl":true,"eager":false}])"));
    librecelik::agent::runSettingsTierImport(gw, t.s);
    ASSERT_EQ(gw.configWrites.size(), 1); // the settings-tier item, and nothing else
    EXPECT_EQ(gw.configWrites[0].first, QStringLiteral("DefaultReason"));
    // No marker is spent on a trust key either: nothing about them was
    // attempted, so nothing about them is recorded as done.
    EXPECT_FALSE(t.s.contains(QStringLiteral("migration/config1Import/TsaUrls")));
    EXPECT_FALSE(t.s.contains(QStringLiteral("migration/config1Import/TslSources")));
    EXPECT_TRUE(librecelik::agent::shouldShowTrustImportNotice(t.s));
}

TEST(SettingsImport, PerItemMarkersRunOnceAndNeverRewriteSucceededItems)
{
    FakeAgentGateway gw;
    TempSettings t;
    t.s.setValue(QStringLiteral("signing/reason"), QStringLiteral("Approval"));
    t.s.setValue(QStringLiteral("signing/location"), QStringLiteral("Niš"));
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_EQ(gw.configWrites.size(), 2);
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_EQ(gw.configWrites.size(), 2); // per-item markers block every re-write
}

TEST(SettingsImport, SemanticRefusalIsTerminalTransportFailureRetries)
{
    FakeAgentGateway gw;
    TempSettings t;
    t.s.setValue(QStringLiteral("signing/reason"), QStringLiteral("Approval"));
    // Semantic refusal → marked attempted, NEVER retried (no nag loop).
    gw.nextRefusal = LibreSCRS::AgentClient::SyncError::InvalidConfigValue;
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_EQ(gw.configWrites.size(), 1);
    gw.nextRefusal.reset();
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_EQ(gw.configWrites.size(), 1); // still one attempt ever
    // Transport-shaped failure → unmarked, retried next Ready.
    t.s.setValue(QStringLiteral("signing/location"), QStringLiteral("Niš"));
    gw.nextRefusal = LibreSCRS::AgentClient::SyncError::CommunicationError;
    librecelik::agent::runSettingsTierImport(gw, t.s);
    gw.nextRefusal.reset();
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_EQ(gw.configWrites.last().first, QStringLiteral("DefaultLocation")); // retried and landed
}

TEST(SettingsImport, EmptyImportSetSkipsTheRunEntirely)
{
    FakeAgentGateway gw;
    TempSettings t; // no legacy keys at all
    librecelik::agent::runSettingsTierImport(gw, t.s);
    EXPECT_TRUE(gw.configWrites.isEmpty());
    EXPECT_FALSE(librecelik::agent::shouldShowTrustImportNotice(t.s)); // nothing to point at
}
