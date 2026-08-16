// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// src/agent/opstallwatchdog.h — the caller-enforced op-stall bound, PHASE-
// AWARE. Mirrors the sole existing consumer of the budget verbatim
// (the KDE host's card data source, armForPhase inside driveToFinished):
// human-in-loop phases (AwaitingConsent, Authenticating — PIN/CAN/consent at
// the system prompter) STOP the timer ("the human is thinking — never time
// out"); machine phases (re)START it, and every phaseChanged/progress tick
// restarts it. NOTE — deliberate deviation from spec §5.4 ("the gateway OWNS
// the op-stall timer"): the timer lives per-op in the CONTROLLERS (and the
// gateway's own DER fetch), because arming decisions need the op's phase
// stream; the gateway still owns the POLICY by constructing every controller.
// Recorded here so the spec deviation is a decision, not drift.
#pragma once
#include <LibreSCRS/AgentClient/ClientTimeouts.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <QObject>
#include <QTimer>

namespace librecelik::agent {

class OpStallWatchdog : public QObject
{
    Q_OBJECT
public:
    explicit OpStallWatchdog(int timeoutMs = LibreSCRS::AgentClient::kLongOperationTimeoutMs,
                             QObject* parent = nullptr);
    /// Arm for the pre-first-signal machine phase (Created).
    void begin();
    /// Feed every AgentOperation::phaseChanged tick (progress ticks restart
    /// the machine-phase timer; consent phases disarm it).
    void onPhase(LibreSCRS::AgentClient::OperationPhase phase, double progress);
    void stop();
signals:
    /// Machine-phase stall exceeded the budget; the owner cancels the op.
    void expired();

private:
    /// The one arming rule, shared by begin() and onPhase().
    void armForPhase(LibreSCRS::AgentClient::OperationPhase phase);

    QTimer timer;
    /// The advisory budget this watchdog enforces, in milliseconds.
    int budgetMs;
};

} // namespace librecelik::agent
