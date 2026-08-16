// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Compile-and-construct coverage for the production gateway. Behaviour
// against a live agent belongs to the dogfood/HW legs — what CI can prove is
// that construction is BOUNDED and that presence resolves to a real state in
// every environment, agent or no agent.

#include "agent/live/liveagentgateway.h"

#include <QElapsedTimer>

#include <gtest/gtest.h>

TEST(LiveAgentGateway, ConstructsAndReportsPresenceWithoutAgent)
{
    // In the offscreen CI environment no agent runs; construction must not
    // hang (bounded probes) and presence must be a valid enum value. The
    // elapsed bound makes the assertion non-vacuous: without it any enum
    // value after ANY delay would pass.
    QElapsedTimer t;
    t.start();
    librecelik::agent::LiveAgentGateway gw;
    const auto p = gw.presence();
    EXPECT_LT(t.elapsed(), 10000); // well under the 3 s + 1 s client caps × margin
    EXPECT_TRUE(p == librecelik::agent::PresenceState::AgentMissing ||
                p == librecelik::agent::PresenceState::AgentUnavailable ||
                p == librecelik::agent::PresenceState::Ready);
    EXPECT_TRUE(gw.readers().isEmpty() || gw.presence() == librecelik::agent::PresenceState::Ready);
}
