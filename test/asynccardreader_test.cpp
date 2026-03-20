// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include "asynccardreader.h"

#include <smartcard/pcsc_connection.h>

#include <QApplication>
#include <QSignalSpy>

namespace {
int argc = 1;
char arg0[] = "test";
char* argv[] = {arg0, nullptr};
QApplication app(argc, argv);
} // namespace

TEST(AsyncCardReaderTest, EmitErrorWhenNoCandidateReads)
{
    std::vector<plugin::CardPlugin*> candidates; // empty

    AsyncCardReader reader(candidates, std::unique_ptr<smartcard::PCSCConnection>());
    QSignalSpy errorSpy(&reader, &AsyncCardReader::errorOccurred);

    reader.requestData();
    ASSERT_TRUE(errorSpy.wait(5000));
    ASSERT_EQ(errorSpy.count(), 1);
}

TEST(AsyncCardReaderTest, EmitErrorWhenNoConnection)
{
    // Mock CardPlugin — never called because conn is null
    class MockPlugin : public plugin::CardPlugin
    {
    public:
        std::string pluginId() const override { return "mock"; }
        std::string displayName() const override { return "Mock"; }
        int probePriority() const override { return 100; }
        bool canHandle(const std::vector<uint8_t>&) const override { return true; }
        plugin::CardData readCard(smartcard::PCSCConnection&) const override { return {}; }
    };

    MockPlugin mockPlugin;
    std::vector<plugin::CardPlugin*> candidates = {&mockPlugin};

    AsyncCardReader reader(candidates, std::unique_ptr<smartcard::PCSCConnection>());
    QSignalSpy errorSpy(&reader, &AsyncCardReader::errorOccurred);

    reader.requestData();
    ASSERT_TRUE(errorSpy.wait(5000));
    ASSERT_EQ(errorSpy.count(), 1);
    EXPECT_TRUE(errorSpy.at(0).at(0).toString().contains("connection"));
}

TEST(AsyncCardReaderTest, CurrentPluginNullBeforeData)
{
    AsyncCardReader reader({}, std::unique_ptr<smartcard::PCSCConnection>());
    EXPECT_EQ(reader.currentPlugin(), nullptr);
}

// NOTE: Tests that exercise the full data flow (EmitCardDataReady,
// EmitCertificatesReady, EmitPinStatusReady) require a real PCSCConnection
// connected to a card reader. These are hardware-dependent integration tests
// that cannot run in CI without a reader. The fallback chain and signal
// marshalling are verified with hardware during manual testing.
