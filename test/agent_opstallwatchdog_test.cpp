// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The arch-critical property is the one a fixed timer gets wrong: a human
// standing at the agent's prompter is not a stalled operation. Both cases are
// scripted — no agent, no card, no hardware — so the rule holds in CI.

#include "agent/opstallwatchdog.h"

#include <LibreSCRS/AgentClient/OperationPhase.h>

#include <QSignalSpy>
#include <QTest>

#include <gtest/gtest.h>

TEST(OpStallWatchdog, LongAwaitingConsentNeverExpires)
{
    using LibreSCRS::AgentClient::OperationPhase;
    librecelik::agent::OpStallWatchdog dog(50); // tight budget for the test
    QSignalSpy expired(&dog, &librecelik::agent::OpStallWatchdog::expired);
    dog.begin();
    dog.onPhase(OperationPhase::AwaitingConsent, 0.0); // human at the prompter
    QTest::qWait(300);                                 // 6× the budget — a fixed timer would have fired
    EXPECT_EQ(expired.count(), 0);
    dog.onPhase(OperationPhase::Signing, 0.0); // machine phase re-arms
    QTest::qWait(300);
    EXPECT_EQ(expired.count(), 1); // now it fires
}

TEST(OpStallWatchdog, ProgressTicksKeepAMachinePhaseAlive)
{
    using LibreSCRS::AgentClient::OperationPhase;
    // Budget ≫ tick interval (400 vs 50 = 8×): a loaded CI runner may
    // deliver a qWait(50) tick 100+ ms late, and a tight 2× ratio turns
    // that scheduling jitter into a spurious expiry (flaky red on correct
    // code). The wide ratio keeps the assertion sharp — expiry would still
    // need a 400 ms tick gap — without trusting CI timing.
    librecelik::agent::OpStallWatchdog dog(400);
    QSignalSpy expired(&dog, &librecelik::agent::OpStallWatchdog::expired);
    dog.begin();
    dog.onPhase(OperationPhase::Signing, 0.0);
    for (int i = 0; i < 12; ++i) { // 600 ms total, ticks every 50 ms
        QTest::qWait(50);
        dog.onPhase(OperationPhase::Signing, i / 12.0); // 12-doc batch heartbeat
    }
    EXPECT_EQ(expired.count(), 0); // per-tick restart: long ≠ stalled
}
