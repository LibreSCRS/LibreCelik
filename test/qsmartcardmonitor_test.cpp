// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "mock_pcsc_scan_provider.h"
#include "smartcard/qsmartcardmonitor.h"

#include <LibreSCRS/SmartCard/MonitorService.h>
#include <LibreSCRS/SmartCard/detail/MonitorInjection.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

using LibreSCRS::SmartCard::MonitorEvent;

TEST(QSmartCardMonitorTest, ConstructDestruct)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    qRegisterMetaType<MonitorEvent>("LibreSCRS::SmartCard::MonitorEvent");

    auto counters = std::make_shared<LibreSCRS::SmartCard::Internal::MockCounters>();
    auto mock = std::make_unique<LibreSCRS::SmartCard::Internal::MockPCSCScanProvider>(counters);
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    auto monitor = LibreSCRS::SmartCard::detail::makeMonitorWithProvider(std::move(mock));
    ASSERT_NE(monitor, nullptr);
    QSmartCardMonitor qtMonitor(*monitor);

    // Wait briefly for monitor thread to start and stop
    QCoreApplication::processEvents();
    QTest::qWait(200);

    SUCCEED();
}

TEST(QSmartCardMonitorTest, CardInsertedSignalEmitted)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    qRegisterMetaType<MonitorEvent>("LibreSCRS::SmartCard::MonitorEvent");

    auto counters = std::make_shared<LibreSCRS::SmartCard::Internal::MockCounters>();
    auto mock = std::make_unique<LibreSCRS::SmartCard::Internal::MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: supported
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card inserted
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0 /* PnP unchanged */}, false});

    // Stop
    mock->pushStatusChange({LONG(SCARD_E_CANCELLED), {}, false});

    auto monitor = LibreSCRS::SmartCard::detail::makeMonitorWithProvider(std::move(mock));
    ASSERT_NE(monitor, nullptr);
    QSmartCardMonitor qtMonitor(*monitor);

    QSignalSpy cardSpy(&qtMonitor, &QSmartCardMonitor::cardEvent);
    QSignalSpy readerSpy(&qtMonitor, &QSmartCardMonitor::readerListChanged);

    ASSERT_TRUE(cardSpy.wait(5000));

    ASSERT_GE(cardSpy.count(), 1);
    auto event = cardSpy.at(0).at(0).value<MonitorEvent>();
    EXPECT_EQ(event.kind, MonitorEvent::Kind::CardInserted);
    EXPECT_EQ(event.readerName, "Reader A");

    EXPECT_GE(readerSpy.count(), 1);
}

TEST(QSmartCardMonitorTest, CardRemovedSignalEmitted)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    qRegisterMetaType<MonitorEvent>("LibreSCRS::SmartCard::MonitorEvent");

    auto counters = std::make_shared<LibreSCRS::SmartCard::Internal::MockCounters>();
    auto mock = std::make_unique<LibreSCRS::SmartCard::Internal::MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});
    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});
    // Card removed
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED, 0}, false});
    // Stop
    mock->pushStatusChange({LONG(SCARD_E_CANCELLED), {}, false});

    auto monitor = LibreSCRS::SmartCard::detail::makeMonitorWithProvider(std::move(mock));
    ASSERT_NE(monitor, nullptr);
    QSmartCardMonitor qtMonitor(*monitor);

    QSignalSpy cardSpy(&qtMonitor, &QSmartCardMonitor::cardEvent);
    ASSERT_TRUE(cardSpy.wait(5000));

    ASSERT_GE(cardSpy.count(), 1);
    auto event = cardSpy.at(0).at(0).value<MonitorEvent>();
    EXPECT_EQ(event.kind, MonitorEvent::Kind::CardRemoved);
    EXPECT_EQ(event.readerName, "Reader A");
}

TEST(QSmartCardMonitorTest, ReaderListContentVerified)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto counters = std::make_shared<LibreSCRS::SmartCard::Internal::MockCounters>();
    auto mock = std::make_unique<LibreSCRS::SmartCard::Internal::MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A", "Reader B"});
    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});
    // Stop after enumeration
    mock->pushStatusChange({LONG(SCARD_E_CANCELLED), {}, false});

    auto monitor = LibreSCRS::SmartCard::detail::makeMonitorWithProvider(std::move(mock));
    ASSERT_NE(monitor, nullptr);
    QSmartCardMonitor qtMonitor(*monitor);

    QSignalSpy readerSpy(&qtMonitor, &QSmartCardMonitor::readerListChanged);
    // The public MonitorService synthesises one ReaderAdded event per reader from the
    // initial snapshot diff; QSmartCardMonitor accumulates these and emits
    // readerListChanged with the growing set each time. The FINAL emission
    // carries the complete reader list — wait until we've accumulated both.
    QTRY_VERIFY_WITH_TIMEOUT(readerSpy.count() >= 2, 5000);

    auto readers = readerSpy.last().at(0).value<QStringList>();
    EXPECT_EQ(readers.size(), 2);
    EXPECT_TRUE(readers.contains("Reader A"));
    EXPECT_TRUE(readers.contains("Reader B"));
}
