// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// gtest main with a QCoreApplication for the agent service-layer suites:
// the watchdog tests drive QTimer through QTest::qWait and the live gateway
// dials the agent over its transport, so both need a live application object
// and an event dispatcher. Without one the timer tests fail one way and pass
// vacuously the other.

#include <QCoreApplication>

#include <gtest/gtest.h>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
