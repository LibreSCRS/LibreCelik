// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/agentbootstrap.h"

#include "utils/libreceliklog.h"

#include <QCoreApplication>
#include <QPointer>

namespace librecelik::agent {

AgentBootstrap::AgentBootstrap(AgentGateway& gateway, AgentStarter& starter, Timing timing, QObject* parent)
    : QObject(parent), gateway(gateway), starter(starter), timing(timing)
{
    pollTimer.setInterval(timing.poll);
    connect(&pollTimer, &QTimer::timeout, this, &AgentBootstrap::onTick);
    connect(&gateway, &AgentGateway::presenceChanged, this, &AgentBootstrap::onPresence);
}

AgentBootstrap::AgentBootstrap(AgentGateway& gateway, AgentStarter& starter, QObject* parent)
    : AgentBootstrap(gateway, starter, Timing{}, parent)
{}

void AgentBootstrap::start()
{
    onPresence(gateway.presence());
}

void AgentBootstrap::retry()
{
    pollTimer.stop();
    runLaunch();
}

Guidance AgentBootstrap::guidance() const
{
    return current;
}

void AgentBootstrap::onPresence(PresenceState state)
{
    if (!starter.canStart())
        return;
    if (state != PresenceState::AgentMissing) {
        pollTimer.stop();
        set(Guidance::None);
        return;
    }
    if (attempted) {
        // Once per session on its own. An agent that was here and has gone
        // again is offered the retry; a launch still under way, or guidance
        // already on screen, stays as it is.
        if (current == Guidance::None)
            set(Guidance::StillMissing);
        return;
    }
    runLaunch();
}

void AgentBootstrap::runLaunch()
{
    attempted = true;
    kicked = false;
    const std::uint64_t generation = ++launchGeneration;
    switch (starter.locate()) {
    case HostLocation::NotFound:
        set(Guidance::NotInstalled);
        return;
    case HostLocation::ElsewhereOnly:
        set(Guidance::NotInApplications);
        return;
    case HostLocation::Installed:
        break;
    }
    set(Guidance::Starting);
    // The completion may arrive on any thread. Hop to the GUI thread first and
    // only there ask whether this object still exists: a QPointer is safe to
    // copy anywhere, but only meaningful to read where the object lives.
    starter.launchHost([guard = QPointer<AgentBootstrap>(this), generation](bool ok) {
        QMetaObject::invokeMethod(
            QCoreApplication::instance(),
            [guard, generation, ok] {
                if (guard && guard->launchGeneration == generation)
                    guard->onLaunched(ok);
            },
            Qt::QueuedConnection);
    });
}

void AgentBootstrap::onLaunched(bool ok)
{
    qCInfo(lcGeneral) << "AgentBootstrap: host launch" << (ok ? "succeeded" : "failed");
    // The agent may have arrived before the completion did; the presence
    // signal has already cleared the guidance then.
    if (gateway.presence() != PresenceState::AgentMissing)
        return;
    if (!ok) {
        set(Guidance::StillMissing);
        return;
    }
    sinceLaunch.start();
    pollTimer.start();
}

void AgentBootstrap::onTick()
{
    gateway.refresh();
    if (gateway.presence() != PresenceState::AgentMissing) {
        pollTimer.stop();
        set(Guidance::None);
        emit presenceSettled();
        return;
    }
    const auto elapsed = std::chrono::milliseconds(sinceLaunch.elapsed());
    if (!kicked && elapsed >= timing.kickAfter) {
        // The host runs but its agent job did not come up with it — a job
        // stopped earlier stays stopped until someone starts it.
        qCInfo(lcGeneral) << "AgentBootstrap: kicking the agent job";
        starter.kickAgent();
        kicked = true;
    }
    if (elapsed >= timing.giveUp) {
        pollTimer.stop();
        set(Guidance::StillMissing);
    }
}

void AgentBootstrap::set(Guidance value)
{
    if (current == value)
        return;
    current = value;
    emit guidanceChanged(current);
}

#ifndef Q_OS_MACOS
std::unique_ptr<AgentStarter> makePlatformAgentStarter()
{
    return std::make_unique<NullAgentStarter>();
}
#endif

} // namespace librecelik::agent
