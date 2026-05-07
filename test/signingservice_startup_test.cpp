// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "settings/settingskeys.h"
#include "signing/signingconfiguration.h"

#include <LibreSCRS/Signing/SigningService.h>
#include <LibreSCRS/Trust/TrustStoreService.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSettings>

namespace {
struct StartupFixture : public ::testing::Test
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

// SigningService is constructed via pure ctor DI taking
// shared_ptr<Trust::TrustStoreService> + TsaProvider. There is no instance()
// factory and no configure*() mutators. This test verifies that the
// SigningConfiguration outputs (makeTrustConfig + makeTsaProvider) are
// acceptable ctor arguments and that construction does not throw on a
// cleared QSettings (empty-user-state path that LC startup exercises).
TEST_F(StartupFixture, ConstructsFromSigningConfiguration)
{
    signing::SigningConfiguration cfg;

    EXPECT_NO_THROW({
        auto trustResult = LibreSCRS::Trust::TrustStoreService::create(cfg.makeTrustConfig());
        ASSERT_TRUE(trustResult.has_value());
        auto trust = *trustResult;
        auto service = std::make_shared<LibreSCRS::Signing::SigningService>(trust, cfg.makeTsaProvider());
        EXPECT_TRUE(service);
    });
}

// Fresh construction yields independent instances — there is no shared
// cache. Callers that want a single shared instance take ownership via
// shared_ptr at their composition root (see LC librecelik.cpp) and inject
// downstream.
TEST_F(StartupFixture, FreshConstructionYieldsIndependentInstances)
{
    signing::SigningConfiguration cfg;
    auto trustResultA = LibreSCRS::Trust::TrustStoreService::create(cfg.makeTrustConfig());
    auto trustResultB = LibreSCRS::Trust::TrustStoreService::create(cfg.makeTrustConfig());
    ASSERT_TRUE(trustResultA.has_value());
    ASSERT_TRUE(trustResultB.has_value());
    auto a = std::make_shared<LibreSCRS::Signing::SigningService>(*trustResultA, cfg.makeTsaProvider());
    auto b = std::make_shared<LibreSCRS::Signing::SigningService>(*trustResultB, cfg.makeTsaProvider());
    EXPECT_NE(a.get(), b.get());
}
