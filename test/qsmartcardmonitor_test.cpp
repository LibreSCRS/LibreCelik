// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0 and LibreSCRS contributors

#include "mock_pcsc_scan_provider.h"
#include "smartcard/qsmartcardmonitor.h"
#include <smartcard/monitor.h>

#include <gtest/gtest.h>

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

TEST(QSmartCardMonitorTest, ConstructDestruct)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    qRegisterMetaType<smartcard::MonitorEvent>("smartcard::MonitorEvent");

    auto counters = std::make_shared<smartcard::MockCounters>();
    auto mock = std::make_unique<smartcard::MockPCSCScanProvider>(counters);
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_UNKNOWN}, false});

    smartcard::Monitor monitor(std::move(mock));
    QSmartCardMonitor qtMonitor(monitor);

    // Wait briefly for monitor thread to start and stop
    QCoreApplication::processEvents();
    QTest::qWait(200);

    SUCCEED();
}

TEST(QSmartCardMonitorTest, CardInsertedSignalEmitted)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    qRegisterMetaType<smartcard::MonitorEvent>("smartcard::MonitorEvent");

    auto counters = std::make_shared<smartcard::MockCounters>();
    auto mock = std::make_unique<smartcard::MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});

    // PnP check: supported
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});

    // Card inserted
    mock->pushStatusChange(
        {SCARD_S_SUCCESS, {SCARD_STATE_PRESENT | SCARD_STATE_CHANGED | (1 << 16), 0 /* PnP unchanged */}, false});

    // Stop
    mock->pushStatusChange({LONG(SCARD_E_CANCELLED), {}, false});

    smartcard::Monitor monitor(std::move(mock));
    QSmartCardMonitor qtMonitor(monitor);

    QSignalSpy cardSpy(&qtMonitor, &QSmartCardMonitor::cardEvent);
    QSignalSpy readerSpy(&qtMonitor, &QSmartCardMonitor::readerListChanged);

    ASSERT_TRUE(cardSpy.wait(5000));

    ASSERT_GE(cardSpy.count(), 1);
    auto event = cardSpy.at(0).at(0).value<smartcard::MonitorEvent>();
    EXPECT_EQ(event.type, smartcard::MonitorEvent::Type::CardInserted);
    EXPECT_EQ(event.readerName, "Reader A");

    EXPECT_GE(readerSpy.count(), 1);
}

TEST(QSmartCardMonitorTest, CardRemovedSignalEmitted)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);
    qRegisterMetaType<smartcard::MonitorEvent>("smartcard::MonitorEvent");

    auto counters = std::make_shared<smartcard::MockCounters>();
    auto mock = std::make_unique<smartcard::MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A"});
    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});
    // Card removed
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_EMPTY | SCARD_STATE_CHANGED, 0}, false});
    // Stop
    mock->pushStatusChange({LONG(SCARD_E_CANCELLED), {}, false});

    smartcard::Monitor monitor(std::move(mock));
    QSmartCardMonitor qtMonitor(monitor);

    QSignalSpy cardSpy(&qtMonitor, &QSmartCardMonitor::cardEvent);
    ASSERT_TRUE(cardSpy.wait(5000));

    ASSERT_GE(cardSpy.count(), 1);
    auto event = cardSpy.at(0).at(0).value<smartcard::MonitorEvent>();
    EXPECT_EQ(event.type, smartcard::MonitorEvent::Type::CardRemoved);
    EXPECT_EQ(event.readerName, "Reader A");
}

TEST(QSmartCardMonitorTest, ReaderListContentVerified)
{
    int argc = 0;
    QCoreApplication app(argc, nullptr);

    auto counters = std::make_shared<smartcard::MockCounters>();
    auto mock = std::make_unique<smartcard::MockPCSCScanProvider>(counters);
    mock->setReaders({"Reader A", "Reader B"});
    // PnP check
    mock->pushStatusChange({SCARD_S_SUCCESS, {SCARD_STATE_CHANGED}, false});
    // Stop after enumeration
    mock->pushStatusChange({LONG(SCARD_E_CANCELLED), {}, false});

    smartcard::Monitor monitor(std::move(mock));
    QSmartCardMonitor qtMonitor(monitor);

    QSignalSpy readerSpy(&qtMonitor, &QSmartCardMonitor::readerListChanged);
    ASSERT_TRUE(readerSpy.wait(5000));

    ASSERT_GE(readerSpy.count(), 1);
    auto readers = readerSpy.at(0).at(0).value<QStringList>();
    EXPECT_EQ(readers.size(), 2);
    EXPECT_EQ(readers[0], "Reader A");
    EXPECT_EQ(readers[1], "Reader B");
}
