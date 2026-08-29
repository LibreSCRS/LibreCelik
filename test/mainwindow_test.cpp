// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen coverage for the main window itself.
///
/// The window used to sit outside every test binary: naming it in a target
/// meant restating the whole application's source list, so its parts were
/// covered and their composition was not. The application object library
/// removes that obstacle — this binary links the same objects the executable
/// ships — and the window takes its gateway from its caller, so a scripted
/// one can drive it with no agent, no D-Bus and no card.

#include "librecelik.h"

#include "agent/agentgateway.h"
#include "agent/agentstatewidget.h"
#include "fake_gateway/fakeagentgateway.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>
#include <QString>

#include <gtest/gtest.h>

#include <memory>

using librecelik::agent::AgentStateWidget;
using librecelik::agent::PresenceState;
using librecelik::test::agent::FakeAgentGateway;

namespace {

/// The shared offscreen application host. Every case here constructs real
/// widgets, and a widget built without a QApplication aborts the process under
/// the offscreen platform — so the fixture owns one. The instance guard keeps
/// it to exactly one per binary.
class OffscreenGuiTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QApplication::instance()) {
            // Static: QApplication keeps a reference to argc for its lifetime,
            // so it must outlive this scope.
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
    }
    static QApplication* app;
};
QApplication* OffscreenGuiTest::app = nullptr;

} // namespace

class MainWindowTest : public OffscreenGuiTest
{};

/// The retry affordance the guided panel offers has to reach the gateway this
/// window actually holds. A window that built its own gateway would refresh
/// that one instead and leave the scripted log empty, which is the whole point
/// of the assertion: it fails on the composition, not on the panel.
TEST_F(MainWindowTest, RetryFromTheGuidedPanelReachesTheInjectedGateway)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    FakeAgentGateway* scripted = gateway.get();
    scripted->scriptedPresence = PresenceState::AgentUnavailable;

    LibreCelik window(std::move(gateway));

    auto* panel = window.findChild<AgentStateWidget*>();
    ASSERT_NE(panel, nullptr);
    auto* retry = panel->findChild<QPushButton*>();
    ASSERT_NE(retry, nullptr);

    retry->click();

    EXPECT_TRUE(scripted->callLog.contains(QStringLiteral("refresh")));
}

/// While the agent is out of reach the window must not claim that no card was
/// detected: it cannot see the readers to know. The guided panel is the only
/// honest copy in that state, so the empty-state text is suppressed.
TEST_F(MainWindowTest, SuppressesTheNoCardCopyWhileTheAgentIsOutOfReach)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    gateway->scriptedPresence = PresenceState::AgentMissing;

    LibreCelik window(std::move(gateway));

    auto* emptyStateCopy = window.findChild<QLabel*>(QStringLiteral("label_3"));
    ASSERT_NE(emptyStateCopy, nullptr);
    EXPECT_TRUE(emptyStateCopy->isHidden());
}

/// The mirror of the case above: once the agent answers, the window is again
/// entitled to say that no card is present, so the copy comes back.
TEST_F(MainWindowTest, RestoresTheNoCardCopyOnceTheAgentAnswers)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    gateway->scriptedPresence = PresenceState::Ready;

    LibreCelik window(std::move(gateway));

    auto* emptyStateCopy = window.findChild<QLabel*>(QStringLiteral("label_3"));
    ASSERT_NE(emptyStateCopy, nullptr);
    EXPECT_FALSE(emptyStateCopy->isHidden());
}
