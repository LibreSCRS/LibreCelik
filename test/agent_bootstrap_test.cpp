// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The missing-agent coordinator, driven by the scripted gateway and a
///        recording starter.
///
/// The starter's completion is held, not answered: each case decides when (and
/// whether) the host "finished launching", because the ordering of that
/// completion against the agent's own arrival is exactly what the flow has to
/// get right.

#include "agent/agentbootstrap.h"
#include "fake_gateway/fakeagentgateway.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <functional>

using librecelik::agent::AgentBootstrap;
using librecelik::agent::AgentStarter;
using librecelik::agent::Guidance;
using librecelik::agent::HostLocation;
using librecelik::agent::NullAgentStarter;
using librecelik::agent::PresenceState;
using librecelik::test::agent::FakeAgentGateway;

namespace {

struct FakeStarter : AgentStarter
{
    HostLocation where = HostLocation::Installed;
    int launches = 0;
    int kicks = 0;
    std::function<void(bool)> pending;

    HostLocation locate() override
    {
        return where;
    }
    void launchHost(std::function<void(bool)> done) override
    {
        ++launches;
        pending = std::move(done);
    }
    void kickAgent() override
    {
        ++kicks;
    }
};

/// Short poll and kick, but a deadline a hundred polls away: a loaded runner
/// may stall the event loop for tens of milliseconds, and a deadline that a
/// stall can reach turns "the poll noticed" into "the poll gave up".
constexpr AgentBootstrap::Timing fast{std::chrono::milliseconds(10), std::chrono::milliseconds(30),
                                      std::chrono::milliseconds(1000)};
/// Every positive wait: well above `fast.giveUp`, so even the deadline case
/// has room on a slow machine. A passing wait returns as soon as it holds.
constexpr int kWaitMs = 5000;

/// The coordinator's completion hop and its poll timer both need a live
/// application object and event dispatcher.
class AgentBootstrapTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QCoreApplication::instance()) {
            static int argc = 0;
            app = new QCoreApplication(argc, nullptr);
        }
    }
    static QCoreApplication* app;

    FakeAgentGateway gateway;
    FakeStarter starter;

    int refreshes() const
    {
        return static_cast<int>(gateway.callLog.count(QStringLiteral("refresh")));
    }
};
QCoreApplication* AgentBootstrapTest::app = nullptr;

} // namespace

TEST_F(AgentBootstrapTest, NotFoundGuidesToInstallAndLaunchesNothing)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    starter.where = HostLocation::NotFound;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    EXPECT_EQ(boot.guidance(), Guidance::NotInstalled);
    EXPECT_EQ(starter.launches, 0);
}

TEST_F(AgentBootstrapTest, ElsewhereOnlyGuidesToApplications)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    starter.where = HostLocation::ElsewhereOnly;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    EXPECT_EQ(boot.guidance(), Guidance::NotInApplications);
    EXPECT_EQ(starter.launches, 0);
}

TEST_F(AgentBootstrapTest, InstalledLaunchesOnceAndShowsStarting)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    QSignalSpy spy(&boot, &AgentBootstrap::guidanceChanged);
    boot.start();
    EXPECT_EQ(boot.guidance(), Guidance::Starting);
    EXPECT_EQ(starter.launches, 1);
    ASSERT_EQ(spy.count(), 1);
    EXPECT_EQ(spy.at(0).at(0).value<Guidance>(), Guidance::Starting);
    // A second missing verdict in the same session launches nothing more.
    gateway.setPresence(PresenceState::AgentUnavailable);
    gateway.setPresence(PresenceState::AgentMissing);
    EXPECT_EQ(starter.launches, 1);
}

TEST_F(AgentBootstrapTest, KicksTheAgentAfterTheLaunchCompletes)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    ASSERT_TRUE(starter.pending);
    // No completion yet: the kick clock has not started, whatever the wall
    // clock says.
    QTest::qWait(60);
    EXPECT_EQ(starter.kicks, 0);
    EXPECT_EQ(refreshes(), 0);

    starter.pending(true);
    EXPECT_TRUE(QTest::qWaitFor([&] { return starter.kicks == 1; }, kWaitMs));
    // One kick per launch, however long the agent then takes.
    EXPECT_TRUE(QTest::qWaitFor([&] { return boot.guidance() == Guidance::StillMissing; }, kWaitMs));
    EXPECT_EQ(starter.kicks, 1);
}

TEST_F(AgentBootstrapTest, ReadyBeforeTheDeadlineClearsGuidanceAndStopsPolling)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    starter.pending(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return refreshes() > 0; }, kWaitMs));
    gateway.setPresence(PresenceState::Ready);
    EXPECT_EQ(boot.guidance(), Guidance::None);
    const int seen = refreshes();
    QTest::qWait(100);
    EXPECT_EQ(refreshes(), seen);
}

TEST_F(AgentBootstrapTest, DeadlineWithoutAgentIsStillMissing)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    starter.pending(true);
    EXPECT_TRUE(QTest::qWaitFor([&] { return boot.guidance() == Guidance::StillMissing; }, kWaitMs));
    EXPECT_GE(refreshes(), 3);
    const int seen = refreshes();
    QTest::qWait(60);
    EXPECT_EQ(refreshes(), seen) << "polling must stop at the deadline";
}

TEST_F(AgentBootstrapTest, LaunchFailureIsStillMissingAtOnce)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    starter.pending(false);
    EXPECT_TRUE(QTest::qWaitFor([&] { return boot.guidance() == Guidance::StillMissing; }, kWaitMs));
    QTest::qWait(60);
    EXPECT_EQ(starter.kicks, 0);
    EXPECT_EQ(refreshes(), 0);
}

TEST_F(AgentBootstrapTest, RetryLaunchesAgain)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    starter.pending(false);
    ASSERT_TRUE(QTest::qWaitFor([&] { return boot.guidance() == Guidance::StillMissing; }, kWaitMs));
    boot.retry();
    EXPECT_EQ(starter.launches, 2);
    EXPECT_EQ(boot.guidance(), Guidance::Starting);
}

TEST_F(AgentBootstrapTest, InitialMissingWithoutASignalStartsTheFlow)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    EXPECT_EQ(starter.launches, 0) << "construction alone must not launch";
    boot.start();
    EXPECT_EQ(starter.launches, 1);
}

TEST_F(AgentBootstrapTest, NullStarterChangesNothing)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    NullAgentStarter null;
    AgentBootstrap boot(gateway, null, fast);
    boot.start();
    gateway.setPresence(PresenceState::AgentUnavailable);
    gateway.setPresence(PresenceState::AgentMissing);
    QTest::qWait(30);
    EXPECT_EQ(boot.guidance(), Guidance::None);
    EXPECT_EQ(refreshes(), 0);
}

TEST_F(AgentBootstrapTest, AgentUnavailableIsNotOurCase)
{
    gateway.scriptedPresence = PresenceState::AgentUnavailable;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    EXPECT_EQ(boot.guidance(), Guidance::None);
    EXPECT_EQ(starter.launches, 0);
}

TEST_F(AgentBootstrapTest, CallbackAfterReadyDoesNotPoll)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    gateway.setPresence(PresenceState::Ready);
    EXPECT_EQ(boot.guidance(), Guidance::None);
    starter.pending(true);
    QTest::qWait(60);
    EXPECT_EQ(refreshes(), 0);
    EXPECT_EQ(starter.kicks, 0);
    EXPECT_EQ(boot.guidance(), Guidance::None);
}

TEST_F(AgentBootstrapTest, AgentUnavailableDuringPollingStopsAndSettles)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    QSignalSpy settled(&boot, &AgentBootstrap::presenceSettled);
    boot.start();
    starter.pending(true);
    ASSERT_TRUE(QTest::qWaitFor([&] { return refreshes() > 0; }, kWaitMs));
    // The live gateway reports no missing -> unavailable move, so the poll is
    // the only thing that can notice it: no signal here on purpose.
    gateway.scriptedPresence = PresenceState::AgentUnavailable;
    EXPECT_TRUE(QTest::qWaitFor([&] { return settled.count() == 1; }, kWaitMs));
    EXPECT_EQ(boot.guidance(), Guidance::None);
    const int seen = refreshes();
    QTest::qWait(60);
    EXPECT_EQ(refreshes(), seen) << "polling must stop once the agent is no longer missing";
    EXPECT_EQ(settled.count(), 1);
}

TEST_F(AgentBootstrapTest, MissingAgainAfterReadyOffersRetry)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    starter.pending(true);
    gateway.setPresence(PresenceState::Ready);
    ASSERT_EQ(boot.guidance(), Guidance::None);
    gateway.setPresence(PresenceState::AgentMissing);
    EXPECT_EQ(boot.guidance(), Guidance::StillMissing);
    EXPECT_EQ(starter.launches, 1) << "a second launch is the user's, not ours";
    boot.retry();
    EXPECT_EQ(starter.launches, 2);
    EXPECT_EQ(boot.guidance(), Guidance::Starting);
}

TEST_F(AgentBootstrapTest, StaleCompletionAfterRetryIsIgnored)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap boot(gateway, starter, fast);
    boot.start();
    auto first = starter.pending;
    boot.retry();
    // The first launch reports failure only now, after the retry started a
    // second one: it must not turn the fresh "Starting" into "StillMissing".
    first(false);
    QTest::qWait(30);
    EXPECT_EQ(boot.guidance(), Guidance::Starting);
}
