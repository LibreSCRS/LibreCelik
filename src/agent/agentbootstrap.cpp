// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/agentbootstrap.h"

#include "utils/libreceliklog.h"

#include <QScopeGuard>

namespace librecelik::agent {

using LibreSCRS::AgentClient::AgentStartResult;

AgentBootstrap::AgentBootstrap(AgentGateway& gateway, Options options, QObject* parent)
    : QObject(parent), gateway(gateway), options(options)
{
    pollTimer.setInterval(options.poll);
    connect(&pollTimer, &QTimer::timeout, this, &AgentBootstrap::onTick);
    connect(&gateway, &AgentGateway::presenceChanged, this, &AgentBootstrap::onPresence);
}

AgentBootstrap::AgentBootstrap(AgentGateway& gateway, QObject* parent) : AgentBootstrap(gateway, Options{}, parent) {}

void AgentBootstrap::start()
{
    onPresence(gateway.presence());
}

void AgentBootstrap::retry()
{
    pollTimer.stop();
    runStart();
}

Guidance AgentBootstrap::guidance() const
{
    return current;
}

bool AgentBootstrap::isStarting() const
{
    return starting;
}

void AgentBootstrap::onPresence(PresenceState state)
{
    if (state == PresenceState::Ready) {
        pollTimer.stop();
        set(Guidance::None);
        return;
    }
    if (pollTimer.isActive())
        return; // a start is under way; its own poll decides
    if (!attempted) {
        runStart();
        return;
    }
    // Once per session on its own. An agent that was here and has gone again
    // is offered the retry; guidance already on screen stays as it is.
    if (state == PresenceState::AgentMissing && current == Guidance::None)
        guide(Guidance::StillMissing);
}

void AgentBootstrap::runStart()
{
    if (starting)
        return; // the start in progress answers for this one too
    attempted = true;
    starting = true;
    emit startingChanged(true);
    const AgentStartResult result = [this] {
        // Ended however the call ends: a start that threw is not in flight, and
        // "Try again" must not stay held for one.
        const auto done = qScopeGuard([this] {
            starting = false;
            emit startingChanged(false);
        });
        return gateway.requestAgentStart();
    }();
    qCInfo(lcGeneral) << "AgentBootstrap: start answered" << static_cast<int>(result);
    switch (result) {
    case AgentStartResult::Running:
        set(Guidance::None);
        break;
    case AgentStartResult::Requested:
        guide(Guidance::Starting);
        sinceStart.start();
        pollTimer.start();
        return;
    case AgentStartResult::NotInstalled:
        guide(Guidance::NotInstalled);
        break;
    case AgentStartResult::NotInApplications:
        guide(Guidance::NotInApplications);
        break;
    case AgentStartResult::Failed:
        guide(Guidance::StillMissing);
        break;
    }
    emit presenceSettled();
}

void AgentBootstrap::onTick()
{
    gateway.refresh();
    if (gateway.presence() == PresenceState::Ready) {
        pollTimer.stop();
        set(Guidance::None);
        emit presenceSettled();
        return;
    }
    if (std::chrono::milliseconds(sinceStart.elapsed()) >= options.giveUp) {
        pollTimer.stop();
        guide(Guidance::StillMissing);
        emit presenceSettled();
    }
}

void AgentBootstrap::guide(Guidance value)
{
    set(options.hostGuidance ? value : Guidance::None);
}

void AgentBootstrap::set(Guidance value)
{
    if (current == value)
        return;
    current = value;
    emit guidanceChanged(current);
}

} // namespace librecelik::agent
