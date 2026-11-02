// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The agent-start coordinator, driven by the scripted gateway.
///
/// The gateway answers `requestAgentStart()` with a scripted result, the way
/// the client library answers it on each platform: final on Linux (the bus
/// started the agent or could not), "requested" on macOS (the host app is
/// launching and the agent arrives later). Host guidance is switched on or off
/// per case, so both platforms' flows run here.

#include "agent/agentbootstrap.h"
#include "fake_gateway/fakeagentgateway.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

#include <stdexcept>

using librecelik::agent::AgentBootstrap;
using librecelik::agent::Guidance;
using librecelik::agent::PresenceState;
using librecelik::test::agent::FakeAgentGateway;
using LibreSCRS::AgentClient::AgentStartResult;

namespace {

/// A short poll, but a deadline a hundred polls away: a loaded runner may stall
/// the event loop for tens of milliseconds, and a deadline that a stall can
/// reach turns "the poll noticed" into "the poll gave up".
AgentBootstrap::Options options(bool hostGuidance)
{
    AgentBootstrap::Options o;
    o.poll = std::chrono::milliseconds(10);
    o.giveUp = std::chrono::milliseconds(1000);
    o.hostGuidance = hostGuidance;
    return o;
}
/// Every positive wait: well above the deadline. A passing wait returns as
/// soon as it holds.
constexpr int kWaitMs = 5000;

/// The poll timer needs a live application object and event dispatcher.
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

    int starts() const
    {
        return static_cast<int>(gateway.callLog.count(QStringLiteral("requestAgentStart")));
    }
    int refreshes() const
    {
        return static_cast<int>(gateway.callLog.count(QStringLiteral("refresh")));
    }
};
QCoreApplication* AgentBootstrapTest::app = nullptr;

} // namespace

/// The Linux cold start: the agent is installed but nothing has started it.
/// Opening the window is the user asking for it, so the window starts it — a
/// window that only looked would sit on "not responding" for good.
TEST_F(AgentBootstrapTest, AStoppedAgentIsStartedWhenTheWindowOpens)
{
    gateway.scriptedPresence = PresenceState::AgentUnavailable;
    gateway.scriptedStartResult = AgentStartResult::Running;
    AgentBootstrap bootstrap(gateway, options(false));
    QSignalSpy settled(&bootstrap, &AgentBootstrap::presenceSettled);

    bootstrap.start();

    EXPECT_EQ(starts(), 1);
    EXPECT_EQ(gateway.presence(), PresenceState::Ready);
    EXPECT_EQ(bootstrap.guidance(), Guidance::None);
    EXPECT_EQ(settled.count(), 1);
}

TEST_F(AgentBootstrapTest, AReadyAgentIsNotStartedAgain)
{
    gateway.scriptedPresence = PresenceState::Ready;
    AgentBootstrap bootstrap(gateway, options(true));
    bootstrap.start();
    EXPECT_EQ(starts(), 0);
}

TEST_F(AgentBootstrapTest, ConstructionAloneStartsNothing)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    AgentBootstrap bootstrap(gateway, options(true));
    EXPECT_EQ(starts(), 0);
}

TEST_F(AgentBootstrapTest, NoHostAnywhereGuidesToInstall)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::NotInstalled;
    AgentBootstrap bootstrap(gateway, options(true));
    bootstrap.start();
    EXPECT_EQ(bootstrap.guidance(), Guidance::NotInstalled);
}

/// The guidance copy names the macOS host; elsewhere the panel's own
/// "not installed" copy and its installation link are the answer.
TEST_F(AgentBootstrapTest, WithoutAHostTheGuidanceStaysThePanelsOwn)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::NotInstalled;
    AgentBootstrap bootstrap(gateway, options(false));
    QSignalSpy settled(&bootstrap, &AgentBootstrap::presenceSettled);
    bootstrap.start();
    EXPECT_EQ(bootstrap.guidance(), Guidance::None);
    EXPECT_EQ(settled.count(), 1);
}

TEST_F(AgentBootstrapTest, AHostOutsideApplicationsGuidesToMoveIt)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::NotInApplications;
    AgentBootstrap bootstrap(gateway, options(true));
    bootstrap.start();
    EXPECT_EQ(bootstrap.guidance(), Guidance::NotInApplications);
}

TEST_F(AgentBootstrapTest, AFailedStartIsStillMissing)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::Failed;
    AgentBootstrap bootstrap(gateway, options(true));
    bootstrap.start();
    EXPECT_EQ(bootstrap.guidance(), Guidance::StillMissing);
}

/// macOS: the host is launching and the agent arrives later. The coordinator
/// says so and polls until the agent answers.
TEST_F(AgentBootstrapTest, ARequestedStartPollsUntilTheAgentAnswers)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::Requested;
    AgentBootstrap bootstrap(gateway, options(true));
    QSignalSpy settled(&bootstrap, &AgentBootstrap::presenceSettled);
    bootstrap.start();
    EXPECT_EQ(bootstrap.guidance(), Guidance::Starting);

    ASSERT_TRUE(QTest::qWaitFor([this] { return refreshes() >= 2; }, kWaitMs));
    gateway.scriptedPresence = PresenceState::Ready; // no signal: the poll must notice
    ASSERT_TRUE(QTest::qWaitFor([&] { return settled.count() == 1; }, kWaitMs));
    EXPECT_EQ(bootstrap.guidance(), Guidance::None);
    const int afterSettle = refreshes();
    QTest::qWait(50);
    EXPECT_EQ(refreshes(), afterSettle) << "polling must stop once the agent answers";
    EXPECT_EQ(starts(), 1);
}

TEST_F(AgentBootstrapTest, ARequestedStartWithoutAnAgentByTheDeadlineIsStillMissing)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::Requested;
    AgentBootstrap bootstrap(gateway, options(true));
    bootstrap.start();
    ASSERT_TRUE(QTest::qWaitFor([&] { return bootstrap.guidance() == Guidance::StillMissing; }, kWaitMs));
}

TEST_F(AgentBootstrapTest, TryAgainStartsAgain)
{
    gateway.scriptedPresence = PresenceState::AgentUnavailable;
    gateway.scriptedStartResult = AgentStartResult::Failed;
    AgentBootstrap bootstrap(gateway, options(false));
    bootstrap.start();
    ASSERT_EQ(starts(), 1);

    gateway.scriptedStartResult = AgentStartResult::Running;
    bootstrap.retry();
    EXPECT_EQ(starts(), 2);
    EXPECT_EQ(gateway.presence(), PresenceState::Ready);
}

/// On Linux the client answers a start only once the bus has started the agent,
/// and runs the event loop meanwhile, so "Try again" can be pressed again while
/// the first press is still being answered. That press asks for nothing more.
TEST_F(AgentBootstrapTest, ATryAgainWhileAStartIsAnsweredAsksForNothingMore)
{
    gateway.scriptedPresence = PresenceState::AgentUnavailable;
    gateway.scriptedStartResult = AgentStartResult::Failed;
    AgentBootstrap bootstrap(gateway, options(false));
    bootstrap.start();
    ASSERT_EQ(starts(), 1);
    ASSERT_FALSE(bootstrap.isStarting());

    QSignalSpy starting(&bootstrap, &AgentBootstrap::startingChanged);
    bool startingDuringTheCall = false;
    int pressesDuringTheCall = 0;
    gateway.duringAgentStart = [&] {
        startingDuringTheCall = bootstrap.isStarting();
        if (pressesDuringTheCall++ == 0)
            bootstrap.retry();
    };
    gateway.scriptedStartResult = AgentStartResult::Running;
    bootstrap.retry();

    EXPECT_EQ(starts(), 2);
    EXPECT_TRUE(startingDuringTheCall);
    EXPECT_FALSE(bootstrap.isStarting());
    EXPECT_EQ(gateway.presence(), PresenceState::Ready);
    ASSERT_EQ(starting.count(), 2);
    EXPECT_TRUE(starting.at(0).at(0).toBool());
    EXPECT_FALSE(starting.at(1).at(0).toBool());
}

/// A start that throws ends like any other: it is no longer in flight, so the
/// next "Try again" reaches the agent rather than being held for it forever.
TEST_F(AgentBootstrapTest, AStartThatThrowsIsNoLongerInFlight)
{
    gateway.scriptedPresence = PresenceState::AgentUnavailable;
    gateway.scriptedStartResult = AgentStartResult::Failed;
    AgentBootstrap bootstrap(gateway, options(false));
    bootstrap.start();
    ASSERT_EQ(starts(), 1);

    QSignalSpy starting(&bootstrap, &AgentBootstrap::startingChanged);
    gateway.duringAgentStart = [] { throw std::runtime_error("transport failed"); };
    EXPECT_THROW(bootstrap.retry(), std::runtime_error);

    EXPECT_FALSE(bootstrap.isStarting());
    ASSERT_EQ(starting.count(), 2);
    EXPECT_FALSE(starting.at(1).at(0).toBool());

    gateway.duringAgentStart = nullptr;
    bootstrap.retry();
    EXPECT_EQ(starts(), 3);
}

/// Once per session on its own: an agent that came and went again is not
/// restarted behind the user's back — they are offered the retry.
TEST_F(AgentBootstrapTest, AnAgentThatLeavesLaterIsOfferedTheRetryNotRestarted)
{
    gateway.scriptedPresence = PresenceState::AgentMissing;
    gateway.scriptedStartResult = AgentStartResult::Running;
    AgentBootstrap bootstrap(gateway, options(true));
    bootstrap.start();
    ASSERT_EQ(starts(), 1);

    gateway.setPresence(PresenceState::AgentMissing);
    EXPECT_EQ(starts(), 1);
    EXPECT_EQ(bootstrap.guidance(), Guidance::StillMissing);
}
