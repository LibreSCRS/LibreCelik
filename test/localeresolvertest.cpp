// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/localeresolver.h"

#include <gtest/gtest.h>

#include <QStringList>

namespace {
const QStringList kSupported = {QStringLiteral("en"), QStringLiteral("sr_RS")};
} // namespace

// Explicit user preference, supported → returned verbatim.
TEST(LocaleResolverTest, ExplicitSupportedPreferenceWins)
{
    EXPECT_EQ(utils::resolveActiveLocale(QStringLiteral("sr_RS"), kSupported, {}), QStringLiteral("sr_RS"));
    EXPECT_EQ(utils::resolveActiveLocale(QStringLiteral("en"), kSupported, {QStringLiteral("sr-RS")}),
              QStringLiteral("en"));
}

// Empty preference + system fallback that maps to a supported code →
// system match (the macOS rc1 reproducer: AppleLanguages =
// ["sr-RS", "sr-Latn-RS", "en-RS"], kLanguage="").
TEST(LocaleResolverTest, EmptyPreferenceFollowsSystemUiLanguage)
{
    const QStringList systemLangs = {QStringLiteral("sr-RS"), QStringLiteral("sr-Latn-RS"), QStringLiteral("en-RS")};
    EXPECT_EQ(utils::resolveActiveLocale({}, kSupported, systemLangs), QStringLiteral("sr_RS"));
}

// Empty preference + system list with no supported entry → English default.
TEST(LocaleResolverTest, EmptyPreferenceUnsupportedSystemFallsToEnglish)
{
    const QStringList systemLangs = {QStringLiteral("zh-CN"), QStringLiteral("ja-JP")};
    EXPECT_EQ(utils::resolveActiveLocale({}, kSupported, systemLangs), QStringLiteral("en"));
}

// Empty preference + empty system list → English default.
TEST(LocaleResolverTest, EmptyEverythingFallsToEnglish)
{
    EXPECT_EQ(utils::resolveActiveLocale({}, kSupported, {}), QStringLiteral("en"));
}

// Explicit-but-unsupported preference does NOT take effect; chain falls
// through to system, then to English. The user's preference is preserved
// elsewhere in QSettings (this helper doesn't write back) so a future
// translation drop can honour it.
TEST(LocaleResolverTest, UnsupportedPreferenceFallsThrough)
{
    EXPECT_EQ(utils::resolveActiveLocale(QStringLiteral("zh_CN"), kSupported, {QStringLiteral("sr-RS")}),
              QStringLiteral("sr_RS"));
    EXPECT_EQ(utils::resolveActiveLocale(QStringLiteral("zh_CN"), kSupported, {}), QStringLiteral("en"));
}

// `QLocale::name()` normalises BCP-47 hyphenated tags (sr-RS) to the
// underscore form (sr_RS) used by QSettings/QTranslator. Verify we hit a
// supported code through this normalisation, not by accident.
TEST(LocaleResolverTest, NormalisesBcp47ToUnderscoreForm)
{
    // BCP-47 "sr-RS" → QLocale::name() "sr_RS" → in supported.
    EXPECT_EQ(utils::resolveActiveLocale({}, kSupported, {QStringLiteral("sr-RS")}), QStringLiteral("sr_RS"));
}

// Pathological case: supportedCodes lacks "en". Caller invariant is
// "non-empty supported list always includes en", but the helper still
// returns supportedCodes.value(0) defensively rather than empty.
TEST(LocaleResolverTest, NoEnglishFallsToFirstSupported)
{
    const QStringList exotic = {QStringLiteral("sr_RS"), QStringLiteral("de_DE")};
    EXPECT_EQ(utils::resolveActiveLocale({}, exotic, {}), QStringLiteral("sr_RS"));
}

// supportedLocaleCodes() invariant: non-empty AND contains "en".
TEST(LocaleResolverTest, SupportedLocaleCodesContractEnglishAlwaysPresent)
{
    const auto supported = utils::supportedLocaleCodes();
    ASSERT_FALSE(supported.isEmpty());
    EXPECT_TRUE(supported.contains(QStringLiteral("en")));
}
