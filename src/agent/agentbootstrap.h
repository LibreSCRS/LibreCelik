// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Brings a missing card agent up through its host application.
///
/// The coordinator watches the gateway's presence. When the agent is missing
/// and the platform has a host that can start it, it locates the host, says
/// what it found, launches it in the background, then polls the gateway: a
/// host that is running but whose agent job stayed down gets one kick, and an
/// agent that still has not answered by the deadline is reported as such,
/// with a retry. All of it runs at most once per session on its own — a retry
/// is always the user's.
///
/// Portable on purpose: the platform half is the `AgentStarter` it is handed,
/// so the whole flow is driven by fakes in the tests.

#pragma once

#include "agent/agentgateway.h"
#include "agent/agentstarter.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <chrono>
#include <cstdint>

namespace librecelik::agent {

class AgentBootstrap : public QObject
{
    Q_OBJECT
public:
    struct Timing
    {
        /// How often the gateway is re-probed after the host launched.
        std::chrono::milliseconds poll{1000};
        /// How long a launched host gets before its agent job is kicked.
        std::chrono::milliseconds kickAfter{3000};
        /// How long after the launch the agent may take before it is reported
        /// as still missing.
        std::chrono::milliseconds giveUp{15000};
    };

    /// @param gateway and @p starter are borrowed and must outlive this object.
    AgentBootstrap(AgentGateway& gateway, AgentStarter& starter, Timing timing, QObject* parent = nullptr);
    AgentBootstrap(AgentGateway& gateway, AgentStarter& starter, QObject* parent = nullptr);

    /// Reads the initial presence(); call once after wiring. The gateway emits
    /// nothing for the state it starts in.
    void start();
    /// The StillMissing button: run the launch path again.
    void retry();
    [[nodiscard]] Guidance guidance() const;

signals:
    void guidanceChanged(librecelik::agent::Guidance guidance);
    /// A poll found the agent no longer missing. The live gateway does not
    /// signal every such move (missing -> unavailable is not a change it
    /// reports), so the window re-reads presence() on this.
    void presenceSettled();

private:
    void onPresence(PresenceState state);
    void runLaunch();
    void onLaunched(bool ok);
    void onTick();
    void set(Guidance value);

    AgentGateway& gateway;
    AgentStarter& starter;
    Timing timing;
    QTimer pollTimer;
    QElapsedTimer sinceLaunch;
    Guidance current = Guidance::None;
    bool attempted = false;
    bool kicked = false;
    /// Bumped by every launch, so a completion that arrives after a retry
    /// already started a newer one is recognised as stale and dropped.
    std::uint64_t launchGeneration = 0;
};

} // namespace librecelik::agent
