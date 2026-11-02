// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Starts the card agent when the window opens without one, and again
///        when the user asks.
///
/// Opening LibreCelik is the user asking for the agent, so the coordinator asks
/// the client library to start it once, on its own, when the window first
/// finds it not running; every later attempt is the user's "Try again". How
/// an agent starts is the library's knowledge, not this window's: on Linux the
/// session bus activates it and the answer is final, on macOS the library
/// launches the LibreMac host and the agent arrives later, so the coordinator
/// polls until it does or a deadline passes.
///
/// Portable on purpose: everything platform-specific is behind the gateway, so
/// the whole flow is driven by the scripted gateway in the tests.

#pragma once

#include "agent/agentgateway.h"

#include <QElapsedTimer>
#include <QObject>
#include <QTimer>

#include <chrono>

namespace librecelik::agent {

/// What the guided panel tells the user while the agent is missing. Every
/// value but None names the macOS host application; elsewhere the panel's own
/// copy covers every case.
enum class Guidance {
    None,              ///< the panel's own "not installed" copy
    Starting,          ///< the host is being launched
    NotInstalled,      ///< no host anywhere: install from the disk image
    NotInApplications, ///< host found only outside Applications: move it
    StillMissing,      ///< launched (or tried), agent never came: retryable
};

class AgentBootstrap : public QObject
{
    Q_OBJECT
public:
    struct Options
    {
        /// How often the gateway is re-probed after a start that answers later.
        std::chrono::milliseconds poll{1000};
        /// How long such a start may take before it is reported as failed.
        std::chrono::milliseconds giveUp{15000};
        /// Whether the agent is started through a host application the guidance
        /// copy names (LibreMac on macOS).
#ifdef Q_OS_MACOS
        bool hostGuidance = true;
#else
        bool hostGuidance = false;
#endif
    };

    /// @param gateway is borrowed and must outlive this object.
    AgentBootstrap(AgentGateway& gateway, Options options, QObject* parent = nullptr);
    explicit AgentBootstrap(AgentGateway& gateway, QObject* parent = nullptr);

    /// Reads the initial presence(); call once after wiring. The gateway emits
    /// nothing for the state it starts in.
    void start();
    /// The panel's "Try again": ask for the agent to be started again. Asks
    /// nothing while a start is still being answered.
    void retry();
    [[nodiscard]] Guidance guidance() const;
    /// Whether the client library is still answering a start. On Linux it
    /// waits for the bus to start the agent and runs the event loop meanwhile,
    /// so the window stays live and its "Try again" can be pressed again.
    [[nodiscard]] bool isStarting() const;

signals:
    void guidanceChanged(librecelik::agent::Guidance guidance);
    /// A start attempt has an answer. The client may have learned the agent's
    /// state without a presence signal (an agent installed since the window
    /// opened is "stopped" now, not "missing"), so the window re-reads
    /// presence() on this.
    void presenceSettled();
    /// A start was asked for (true) or has its answer (false).
    void startingChanged(bool starting);

private:
    void onPresence(PresenceState state);
    void runStart();
    void onTick();
    void set(Guidance value);
    /// @p value where the guidance names the host, None everywhere else.
    void guide(Guidance value);

    AgentGateway& gateway;
    Options options;
    QTimer pollTimer;
    QElapsedTimer sinceStart;
    Guidance current = Guidance::None;
    bool attempted = false;
    bool starting = false;
};

} // namespace librecelik::agent
