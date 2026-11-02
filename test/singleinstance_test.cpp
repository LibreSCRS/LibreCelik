// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/singleinstance.h"

#include <QCoreApplication>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include <gtest/gtest.h>

using librecelik::utils::SingleInstance;

namespace {

class SingleInstanceTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            app = new QCoreApplication(argc, nullptr);
        }
        ASSERT_TRUE(dir.isValid());
    }
    [[nodiscard]] QString socketPath() const
    {
        return dir.filePath(QStringLiteral("librecelik.socket"));
    }
    static QCoreApplication* app;
    QTemporaryDir dir;
};
QCoreApplication* SingleInstanceTest::app = nullptr;

} // namespace

/// The first start is the instance; a second start finds it, asks it to show
/// its window, and is told to exit — one window, not two.
TEST_F(SingleInstanceTest, ASecondStartRaisesTheFirstAndExits)
{
    SingleInstance first(socketPath());
    ASSERT_TRUE(first.claim());
    QSignalSpy raised(&first, &SingleInstance::activationRequested);

    SingleInstance second(socketPath());
    EXPECT_FALSE(second.claim());

    EXPECT_TRUE(raised.wait(2000));
    EXPECT_EQ(raised.count(), 1);
}

/// A socket file left behind by an instance that did not exit cleanly answers
/// nobody; the next start reclaims it instead of exiting into nothing.
TEST_F(SingleInstanceTest, ALeftoverSocketFileDoesNotStopTheNextStart)
{
    {
        QFile stale(socketPath());
        ASSERT_TRUE(stale.open(QIODevice::WriteOnly));
    }
    SingleInstance next(socketPath());
    EXPECT_TRUE(next.claim());

    SingleInstance after(socketPath());
    EXPECT_FALSE(after.claim()) << "the reclaimed name must now be answered";
}
