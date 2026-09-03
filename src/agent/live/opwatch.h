// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include "agent/errortext.h"
#include "agent/opstallwatchdog.h"

#include <LibreSCRS/AgentClient/AgentOperation.h>
#include <LibreSCRS/AgentClient/ClientTimeouts.h>

#include <QObject>
#include <QString>

#include <memory>
#include <utility>

/// @file
/// @brief The per-operation stall bound the live controllers arm, in one place.
///
/// Header-only and stateless, next to the two controllers that arm it.

namespace librecelik::agent {

/// @brief A watchdog armed on one operation, plus the flag its expiry sets.
///
/// The flag is shared rather than owned by the watchdog because it outlives
/// the expiry it records: the operation's terminal arrives LATER (a cancel is
/// fire-and-forget), and by then the only evidence that the terminal was a
/// stall and not a card error is this flag.
struct OpWatch
{
    OpStallWatchdog* dog = nullptr;
    std::shared_ptr<bool> fired;
};

/// @brief Arm a per-operation stall bound.
///
/// Parented to the operation (so it cannot outlive it), fed by the operation's
/// own phase stream, cancelling on expiry. Phase-aware by construction — the
/// consent and authentication phases, where the human is at the prompter,
/// never time out; a timestamp leg is a long MACHINE phase and restarts the
/// budget on every tick.
[[nodiscard]] inline OpWatch armWatchdog(LibreSCRS::AgentClient::AgentOperation* operation)
{
    using LibreSCRS::AgentClient::AgentOperation;
    auto* dog = new OpStallWatchdog(LibreSCRS::AgentClient::kLongOperationTimeoutMs, operation);
    auto fired = std::make_shared<bool>(false);
    QObject::connect(operation, &AgentOperation::phaseChanged, dog, &OpStallWatchdog::onPhase);
    QObject::connect(dog, &OpStallWatchdog::expired, operation, [operation, fired] {
        *fired = true;
        // Fire-and-forget by contract: the terminal still arrives via
        // finished(), which is the only place an outcome is ever read.
        operation->cancel();
    });
    dog->begin();
    return OpWatch{dog, std::move(fired)};
}

/// @brief The line for a verb issued against a card the client has already removed.
[[nodiscard]] inline QString cardGoneText()
{
    return errorText(LibreSCRS::AgentClient::ErrorCode::CardRemoved, LibreSCRS::AgentClient::CallError::None, {}, {});
}

} // namespace librecelik::agent
