// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The macOS starter: LibreMac hosts the card agent as a launchd job.
///
/// Locating, launching and kicking go through NSWorkspace and launchctl, so
/// the host is found wherever Launch Services knows it, launched without
/// stealing focus, and its agent job is started if the host came up without
/// it. Nothing here touches a card or a credential.

#pragma once

#include "agent/agentstarter.h"

#include <QString>

namespace librecelik::agent {

class MacAgentStarter final : public AgentStarter
{
public:
    [[nodiscard]] HostLocation locate() override;
    /// Launches the host `locate()` last found installed; done(false) when it
    /// found none.
    void launchHost(std::function<void(bool)> done) override;
    void kickAgent() override;

private:
    /// The installed host's bundle path from the last `locate()`, empty when
    /// it found none in Applications.
    QString installedHost;
};

} // namespace librecelik::agent
