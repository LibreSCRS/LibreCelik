// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The platform seam that can bring a missing card agent up.
///
/// On macOS the agent is a launchd job registered by the LibreMac host app,
/// so "the agent is missing" often means "LibreMac has not run yet" or "its
/// job was stopped" — both things LibreCelik can fix by launching the host
/// and kicking the job. On every other platform there is nothing to start
/// (the agent is a system component), and the null starter keeps the
/// coordinator idle there.

#pragma once

#include <functional>
#include <memory>

namespace librecelik::agent {

/// Where the agent's host application was found.
enum class HostLocation {
    Installed,     ///< in /Applications or ~/Applications — launchable
    ElsewhereOnly, ///< only outside Applications (a mounted image, Downloads)
    NotFound,      ///< nowhere the system knows of
};

class AgentStarter
{
public:
    virtual ~AgentStarter() = default;

    /// false for a platform with nothing to start (the coordinator stays idle).
    [[nodiscard]] virtual bool canStart() const
    {
        return true;
    }
    [[nodiscard]] virtual HostLocation locate() = 0;
    /// Launch the host in the background; @p done(true) once it is running,
    /// done(false) if it could not be launched (refused, failed). @p done may
    /// be called on any thread.
    virtual void launchHost(std::function<void(bool)> done) = 0;
    /// Start an already-registered agent job; harmless when none exists.
    virtual void kickAgent() = 0;
};

class NullAgentStarter final : public AgentStarter
{
public:
    [[nodiscard]] bool canStart() const override
    {
        return false;
    }
    [[nodiscard]] HostLocation locate() override
    {
        return HostLocation::NotFound;
    }
    void launchHost(std::function<void(bool)> done) override
    {
        done(false);
    }
    void kickAgent() override {}
};

/// What the guided panel tells the user while the agent is missing.
enum class Guidance {
    None,              ///< the panel's own "not installed" copy
    Starting,          ///< the host is being launched
    NotInstalled,      ///< no host anywhere: install from the disk image
    NotInApplications, ///< host found only outside Applications: move it
    StillMissing,      ///< launched (or tried), agent never came: retryable
};

/// The starter for the platform this build targets.
[[nodiscard]] std::unique_ptr<AgentStarter> makePlatformAgentStarter();

} // namespace librecelik::agent
