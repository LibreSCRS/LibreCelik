// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/agentgateway.h"
#include <gtest/gtest.h>

using librecelik::agent::PresenceState;
using librecelik::agent::resolvePresence;

TEST(ResolvePresence, AvailableWinsRegardlessOfInstalled)
{
    EXPECT_EQ(resolvePresence(true, true), PresenceState::Ready);
    EXPECT_EQ(resolvePresence(false, true), PresenceState::Ready);
}
TEST(ResolvePresence, InstalledButUnreachableIsUnavailable)
{
    EXPECT_EQ(resolvePresence(true, false), PresenceState::AgentUnavailable);
}
TEST(ResolvePresence, NotInstalledIsMissing)
{
    EXPECT_EQ(resolvePresence(false, false), PresenceState::AgentMissing);
}
