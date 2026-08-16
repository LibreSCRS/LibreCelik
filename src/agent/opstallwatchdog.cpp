// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The client library has NO internal watchdog and never auto-fails a stalled
// operation (ClientTimeouts.h says so explicitly), so every consumer that
// wants stall detection runs its own timer. This is LC's, and the ONE thing it
// must get right is that a human standing at the agent's prompter is not a
// stall: an operation waiting on consent may legitimately take minutes, and
// cancelling it there would look to the user like the card giving up while
// they typed.

#include "agent/opstallwatchdog.h"

namespace librecelik::agent {

using LibreSCRS::AgentClient::OperationPhase;

OpStallWatchdog::OpStallWatchdog(int timeoutMs, QObject* parent) : QObject(parent), budgetMs(timeoutMs)
{
    timer.setSingleShot(true);
    connect(&timer, &QTimer::timeout, this, &OpStallWatchdog::expired);
}

void OpStallWatchdog::begin()
{
    // Created is a real phase, not a sentinel: an operation reports it until
    // its first progress notification arrives, and that window is machine work
    // like any other — bounded from the moment the operation is minted.
    armForPhase(OperationPhase::Created);
}

void OpStallWatchdog::onPhase(OperationPhase phase, double progress)
{
    // The progress fraction is deliberately unused: a tick is a heartbeat
    // whatever fraction it carries, and a batch that reports the same fraction
    // twice is still alive.
    Q_UNUSED(progress)
    armForPhase(phase);
}

void OpStallWatchdog::stop()
{
    timer.stop();
}

void OpStallWatchdog::armForPhase(OperationPhase phase)
{
    const bool humanInLoop = (phase == OperationPhase::AwaitingConsent || phase == OperationPhase::Authenticating);
    if (humanInLoop) {
        timer.stop(); // the human is thinking at the prompter — never time out
    } else {
        timer.start(budgetMs); // machine phase — bound it, reset on each tick
    }
}

} // namespace librecelik::agent
