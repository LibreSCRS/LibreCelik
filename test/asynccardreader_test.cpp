// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include "asynccardreader.h"

#include <LibreSCRS/SmartCard/CardSession.h>

#include <QApplication>
#include <QSignalSpy>

#include <array>
#include <span>
#include <stop_token>

namespace {
int argc = 1;
char arg0[] = "test";
char* argv[] = {arg0, nullptr};
QApplication app(argc, argv);
} // namespace

TEST(AsyncCardReaderTest, EmitErrorWhenNoCandidateReads)
{
    std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> candidates; // empty

    AsyncCardReader reader(candidates, {}, std::shared_ptr<LibreSCRS::SmartCard::CardSession>());
    QSignalSpy errorSpy(&reader, &AsyncCardReader::errorOccurred);

    reader.requestData();
    ASSERT_TRUE(errorSpy.wait(5000));
    ASSERT_EQ(errorSpy.count(), 1);
}

TEST(AsyncCardReaderTest, EmitErrorWhenNoConnection)
{
    // Mock CardPlugin — never called because session is null.
    //
    // LM 4.0 NVI: override doReadCard (protected, virtual), not readCard
    // (public, non-virtual). Plus supportedAtrs is now the abstract probe
    // hook; canHandle became non-virtual final at the base.
    class MockPlugin : public LibreSCRS::Plugin::CardPlugin
    {
    public:
        MockPlugin()
        {
            setIdentity("mock", "Mock", /*priority=*/100);
        }
        LibreSCRS::Plugin::CardCapabilities capabilities() const override
        {
            return LibreSCRS::Plugin::CardCapabilities::None;
        }
        std::span<const LibreSCRS::Plugin::Atr> supportedAtrs() const noexcept override
        {
            static const std::array<LibreSCRS::Plugin::Atr, 0> kEmpty{};
            return kEmpty;
        }

    protected:
        LibreSCRS::Plugin::ReadResult doReadCard(LibreSCRS::SmartCard::CardSession&, GroupCallback) const override
        {
            return LibreSCRS::Plugin::ReadResult::ok(LibreSCRS::Plugin::CardData{});
        }
    };

    auto mockPlugin = std::make_shared<MockPlugin>();
    std::vector<std::shared_ptr<LibreSCRS::Plugin::CardPlugin>> candidates = {mockPlugin};

    AsyncCardReader reader(candidates, {}, std::shared_ptr<LibreSCRS::SmartCard::CardSession>());
    QSignalSpy errorSpy(&reader, &AsyncCardReader::errorOccurred);

    reader.requestData();
    ASSERT_TRUE(errorSpy.wait(5000));
    ASSERT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.at(0).at(0).toString().contains("connection"));
}

TEST(AsyncCardReaderTest, CurrentPluginNullBeforeData)
{
    AsyncCardReader reader({}, {}, std::shared_ptr<LibreSCRS::SmartCard::CardSession>());
    EXPECT_EQ(reader.currentPlugin(), nullptr);
}

// NOTE: Tests that exercise the full data flow (EmitCardDataReady,
// EmitCertificatesReady, EmitPinStatusReady) require a real CardSession
// connected to a card reader. These are hardware-dependent integration tests
// that cannot run in CI without a reader. The fallback chain and signal
// marshalling are verified with hardware during manual testing.

TEST(PinStatusEntryTest, DefaultValues)
{
    LibreSCRS::Plugin::PinStatusEntry entry;
    EXPECT_TRUE(entry.label.empty());
    EXPECT_EQ(entry.reference, 0);
    EXPECT_FALSE(entry.retriesLeft.has_value());
    EXPECT_TRUE(entry.initialized);
    EXPECT_FALSE(entry.blocked);
}

TEST(PinStatusEntryTest, TransportPin)
{
    LibreSCRS::Plugin::PinStatusEntry entry;
    entry.label = "Signature PIN";
    entry.reference = 0x82;
    entry.initialized = false;
    entry.blocked = false;
    EXPECT_EQ(entry.label, "Signature PIN");
    EXPECT_EQ(entry.reference, 0x82);
    EXPECT_FALSE(entry.initialized);
    EXPECT_FALSE(entry.blocked);
}

TEST(PinStatusEntryTest, BlockedPin)
{
    LibreSCRS::Plugin::PinStatusEntry entry;
    entry.label = "User PIN";
    entry.reference = 0x81;
    entry.retriesLeft = 0;
    entry.initialized = true;
    entry.blocked = true;
    EXPECT_TRUE(entry.blocked);
    ASSERT_TRUE(entry.retriesLeft.has_value());
    EXPECT_EQ(*entry.retriesLeft, 0);
}

TEST(CardPluginTest, MinimalPluginDoesNotSupportPKI)
{
    // LM 4.0 NVI shape: see MockPlugin in EmitErrorWhenNoConnection above.
    class MinimalPlugin : public LibreSCRS::Plugin::CardPlugin
    {
    public:
        MinimalPlugin()
        {
            setIdentity("minimal", "Minimal", /*priority=*/100);
        }
        LibreSCRS::Plugin::CardCapabilities capabilities() const override
        {
            return LibreSCRS::Plugin::CardCapabilities::None;
        }
        std::span<const LibreSCRS::Plugin::Atr> supportedAtrs() const noexcept override
        {
            static const std::array<LibreSCRS::Plugin::Atr, 0> kEmpty{};
            return kEmpty;
        }

    protected:
        LibreSCRS::Plugin::ReadResult doReadCard(LibreSCRS::SmartCard::CardSession&, GroupCallback) const override
        {
            return LibreSCRS::Plugin::ReadResult::ok(LibreSCRS::Plugin::CardData{});
        }
    };

    MinimalPlugin plugin;
    EXPECT_FALSE(LibreSCRS::Plugin::hasCapability(plugin.capabilities(), LibreSCRS::Plugin::CardCapabilities::PKI));
}
