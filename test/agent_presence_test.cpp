// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/agentgateway.h"

#include <gtest/gtest.h>

using librecelik::agent::presenceFor;
using librecelik::agent::PresenceState;
using LibreSCRS::AgentClient::AgentState;

TEST(AgentPresence, ReadyIsReady)
{
    EXPECT_EQ(presenceFor(AgentState::Ready), PresenceState::Ready);
}

TEST(AgentPresence, AnInstalledAgentThatIsNotRunningIsUnavailable)
{
    EXPECT_EQ(presenceFor(AgentState::Stopped), PresenceState::AgentUnavailable);
}

TEST(AgentPresence, NoAgentIsMissing)
{
    EXPECT_EQ(presenceFor(AgentState::Missing), PresenceState::AgentMissing);
}
