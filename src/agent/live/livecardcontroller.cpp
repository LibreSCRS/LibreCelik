// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/live/livecardcontroller.h"

#include "agent/errortext.h"
#include "agent/fieldmerge.h"
#include "agent/opstallwatchdog.h"
#include "agent/optionalsections.h" // the ONE spelling of the feature tokens

#include <LibreSCRS/AgentClient/AgentCapabilities.h>
#include <LibreSCRS/AgentClient/AgentCard.h>
#include <LibreSCRS/AgentClient/AgentClient.h>
#include <LibreSCRS/AgentClient/AgentOperation.h>
#include <LibreSCRS/AgentClient/ClientTimeouts.h>

#include <QLatin1StringView>

#include <memory>
#include <utility>

namespace librecelik::agent {

using LibreSCRS::AgentClient::AgentCard;
using LibreSCRS::AgentClient::AgentOperation;
using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::FieldGroup;
using LibreSCRS::AgentClient::OperationStatus;
using LibreSCRS::AgentClient::PreReadAuth;
/// `Cap` is a namespace of bit constants, not a type — an alias, not a
/// using-declaration.
namespace Cap = LibreSCRS::AgentClient::Cap;

namespace {

/// A watchdog armed on one operation, plus the flag its expiry sets.
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

/// Arm a per-operation stall bound: parented to the operation (so it cannot
/// outlive it), fed by the operation's own phase stream, cancelling on expiry.
[[nodiscard]] OpWatch armWatchdog(AgentOperation* operation)
{
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

/// The localized line for a failed terminal.
///
/// A caller-enforced stall has no wire error of its own — the operation
/// reports Cancelled with no code, which would otherwise render as the
/// generic line and read as final. The agent's own watchdog copy is the honest
/// text for it: the same failure, and it reads as retryable.
[[nodiscard]] QString terminalText(AgentOperation* operation, const OpWatch& watch)
{
    if (watch.fired && *watch.fired) {
        return errorText(ErrorCode::WatchdogTimeout, CallError::None, {}, {});
    }
    return errorText(operation->errorCode(), operation->callError(), operation->messageKey(),
                     operation->messageFallback());
}

/// The line for a verb issued against a card the client has already removed.
[[nodiscard]] QString cardGoneText()
{
    return errorText(ErrorCode::CardRemoved, CallError::None, {}, {});
}

/// How many fields the model's photo group carries — the before/after measure
/// that says whether a merge actually added anything.
[[nodiscard]] qsizetype photoFieldCount(const QList<FieldGroup>& groups)
{
    for (const FieldGroup& group : groups) {
        if (group.key == QLatin1StringView("photo")) {
            return group.fields.size();
        }
    }
    return 0;
}

} // namespace

LiveCardController::LiveCardController(AgentCard* cardProxy, LibreSCRS::AgentClient::AgentClient* agentClient,
                                       QObject* parent)
    : CardController(parent), card(cardProxy), client(agentClient)
{
    if (card.isNull()) {
        return;
    }
    // cardType() is empty until a read resolves a multi-candidate card, and
    // plugin dispatch keys on it, so the flip is a first-class event rather
    // than something a consumer polls for.
    connect(card.data(), &AgentCard::cardTypeChanged, this, [this] {
        if (!card.isNull()) {
            Q_EMIT cardTypeResolved(card->cardType());
        }
    });
}

LiveCardController::~LiveCardController() = default;

QString LiveCardController::cardId() const
{
    return card.isNull() ? QString() : card->id();
}

QString LiveCardController::readerId() const
{
    return card.isNull() ? QString() : card->readerId();
}

QString LiveCardController::cardType() const
{
    return card.isNull() ? QString() : card->cardType();
}

QString LiveCardController::atrHex() const
{
    return card.isNull() ? QString() : card->atrHex();
}

std::uint32_t LiveCardController::capabilityBits() const
{
    return card.isNull() ? Cap::None : LibreSCRS::AgentClient::capabilityBits(card->capabilities());
}

PreReadAuth LiveCardController::preReadAuth() const
{
    return card.isNull() ? PreReadAuth::None : LibreSCRS::AgentClient::preReadAuthFromToken(card->preReadAuth());
}

void LiveCardController::startRead()
{
    Q_EMIT readingStarted();
    if (card.isNull()) {
        failRead(cardGoneText(), ErrorCode::CardRemoved);
        return;
    }

    AgentOperation* operation = card->readIdentity();
    track(operation);
    // Progressive delivery straight through: the client's groups are already
    // the LC field model, so there is nothing to convert on this path.
    connect(operation, &AgentOperation::groupReady, this, &CardController::groupReady);
    const OpWatch watch = armWatchdog(operation);

    connect(operation, &AgentOperation::finished, this, [this, operation, watch] {
        watch.dog->stop(); // a completed operation must never leave a live timer
        forget(operation);
        const bool ok = operation->status() == OperationStatus::Ok;
        const QString failure = ok ? QString() : terminalText(operation, watch);
        QList<FieldGroup> groups = operation->identityResult();
        operation->deleteLater();

        if (!ok) {
            failRead(failure, operation->errorCode());
            return;
        }
        chainPhoto(std::move(groups));
    });
}

void LiveCardController::chainPhoto(QList<FieldGroup> groups)
{
    // Gated on IdentityData because that is the capability the agent gates
    // GetPhoto on: asking a PKI-only card for a portrait buys a refusal, not a
    // photo, and would turn a healthy read into an error line.
    if (card.isNull() || !LibreSCRS::AgentClient::has(capabilityBits(), Cap::IdentityData)) {
        finishRead(groups, false);
        return;
    }

    AgentOperation* operation = card->getPhoto();
    track(operation);
    const OpWatch watch = armWatchdog(operation);

    connect(operation, &AgentOperation::finished, this, [this, operation, watch, groups = std::move(groups)]() mutable {
        watch.dog->stop();
        forget(operation);
        bool merged = false;
        if (operation->status() == OperationStatus::Ok) {
            const qsizetype before = photoFieldCount(groups);
            groups = mergePhotoIntoGroups(std::move(groups), operation->takePhotos());
            merged = photoFieldCount(groups) > before;
        }
        // A failed or empty photo fetch is NOT a failed read: the
        // identity model is complete and a card without a portrait
        // renders the placeholder. Failing here would throw away a
        // successful read over its least essential field.
        operation->deleteLater();
        finishRead(groups, merged);
    });
}

void LiveCardController::finishRead(const QList<FieldGroup>& groups, bool photoMerged)
{
    if (photoMerged) {
        // The photo group is streamed FIRST, as the final groupReady: a
        // progressive consumer never sees it otherwise (it arrives from a
        // separate operation, after identity streaming has ended), and would
        // render photoless forever.
        for (const FieldGroup& group : groups) {
            if (group.key == QLatin1StringView("photo")) {
                Q_EMIT groupReady(group);
                break;
            }
        }
    }
    Q_EMIT identityReady(groups);
    Q_EMIT readingFinished();
}

void LiveCardController::failRead(const QString& message, ErrorCode code)
{
    Q_EMIT errorOccurred(message, code);
    Q_EMIT readingFinished();
}

void LiveCardController::requestCertificates()
{
    if (card.isNull()) {
        Q_EMIT errorOccurred(cardGoneText(), ErrorCode::CardRemoved);
        return;
    }

    // Opportunistic warm alongside the read it accompanies: the agent dedups
    // overlapping certificate reads onto one card read, so this costs nothing
    // here and leaves the agent's cache warm for the next consumer of this
    // card — including a different component of the same desktop session.
    card->warmCertificates();

    AgentOperation* operation = card->readCertificates();
    track(operation);
    const OpWatch watch = armWatchdog(operation);

    connect(operation, &AgentOperation::finished, this, [this, operation, watch] {
        watch.dog->stop();
        forget(operation);
        if (operation->status() == OperationStatus::Ok) {
            Q_EMIT certificatesReady(operation->certificatesResult());
        } else {
            Q_EMIT errorOccurred(terminalText(operation, watch), operation->errorCode());
        }
        operation->deleteLater();
    });
}

void LiveCardController::requestTokenInfo()
{
    // Feature double-guard. An agent predating token info refuses the verb
    // LOUDLY (the client fails ReadTokenInfo at entry), so without this gate
    // every insertion against such an agent surfaces a capability error for a
    // block LC should simply not show. The empty group is a SUCCESS shape.
    if (client == nullptr || !client->hasFeature(kTokenInfoFeature)) {
        Q_EMIT tokenInfoReady(FieldGroup{QStringLiteral("token"), {}, {}});
        return;
    }
    if (card.isNull()) {
        Q_EMIT errorOccurred(cardGoneText(), ErrorCode::CardRemoved);
        return;
    }

    AgentOperation* operation = card->readTokenInfo();
    track(operation);
    const OpWatch watch = armWatchdog(operation);

    connect(operation, &AgentOperation::finished, this, [this, operation, watch] {
        watch.dog->stop();
        forget(operation);
        if (operation->status() == OperationStatus::Ok) {
            // readTokenInfo binds the SAME accessor readIdentity() does: a
            // single "token" group. A plugin without token info answers a
            // present-but-empty group, which is still a success.
            FieldGroup tokenGroup{QStringLiteral("token"), {}, {}};
            const QList<FieldGroup> groups = operation->identityResult();
            for (const FieldGroup& group : groups) {
                if (group.key == QLatin1StringView("token")) {
                    tokenGroup = group;
                    break;
                }
            }
            Q_EMIT tokenInfoReady(tokenGroup);
        } else {
            // An ancillary surface hides on failure, exactly like the
            // absent-feature guard above — errorOccurred is the READ's
            // channel, and the window kills a still-spinning page on it
            // (the Leg-5 bench catch: a per-card refusal raced the running
            // identity read and released its page).
            Q_EMIT tokenInfoReady(FieldGroup{QStringLiteral("token"), {}, {}});
        }
        operation->deleteLater();
    });
}

void LiveCardController::requestCredentials()
{
    // The same double-guard as token info: the socket transport drops
    // credential frames against an agent that never advertised them, so an
    // ungated listing is an error line for a section LC should hide.
    if (client == nullptr || !client->hasFeature(kCredentialsFeature)) {
        Q_EMIT credentialsReady({});
        return;
    }
    if (card.isNull()) {
        Q_EMIT errorOccurred(cardGoneText(), ErrorCode::CardRemoved);
        return;
    }

    AgentOperation* operation = card->listCredentials();
    track(operation);
    const OpWatch watch = armWatchdog(operation);

    connect(operation, &AgentOperation::finished, this, [this, operation, watch] {
        watch.dog->stop();
        forget(operation);
        if (operation->status() == OperationStatus::Ok) {
            // An empty list is a legitimate result, not a missing one.
            Q_EMIT credentialsReady(operation->credentialsResult());
        } else {
            // Same hide-on-failure posture as token info above: the
            // credentials block simply does not render, and the page the
            // identity read is filling stays alive.
            Q_EMIT credentialsReady({});
        }
        operation->deleteLater();
    });
}

void LiveCardController::managePin(const QString& pinId, LibreSCRS::AgentClient::PinVerb verb,
                                   const LibreSCRS::AgentClient::ManagePinOptions& options)
{
    if (card.isNull()) {
        Q_EMIT errorOccurred(cardGoneText(), ErrorCode::CardRemoved);
        return;
    }
    AgentOperation* operation = card->managePin(pinId, verb, options);
    track(operation);
    const OpWatch watch = armWatchdog(operation);
    // The mutation's phase stream is forwarded to the human as well as to the
    // watchdog: this is the one verb where the wait is long, interactive, and
    // owned by a dialog that has a row to put it in.
    connect(operation, &AgentOperation::phaseChanged, this, &CardController::pinPhaseChanged);

    connect(operation, &AgentOperation::finished, this, [this, operation, watch, pinId, verb, options] {
        watch.dog->stop();
        forget(operation);
        // The agent forgets a card's credential listing after an idle window,
        // and a dialog left open across it still holds the ids from before.
        // That refusal carries its own name so a client can re-list and try
        // again instead of showing a failure repeating would have fixed —
        // nothing was presented to the card, so the retry costs no attempt.
        // Once only: a second identical refusal is a real disagreement, not a
        // stale snapshot.
        const bool staleIds = operation->syncError() == LibreSCRS::AgentClient::SyncError::UnknownCredential;
        const auto result = operation->pinResult(); // read BEFORE scheduling deletion
        operation->deleteLater();
        if (staleIds && !pinRetryUsed) {
            pinRetryUsed = true;
            requestCredentials(); // repopulates the agent's snapshot
            managePin(pinId, verb, options);
            return;
        }
        pinRetryUsed = false;
        // ALWAYS: the client delivers a PinResult even for the soft-fail
        // outcomes that finish Error (a wrong PIN, a blocked credential), and
        // the outcome — with its retry counter — is exactly what the caller
        // must render. An error line here would hide it behind a generic
        // failure.
        Q_EMIT pinResultReady(result);
        // Mandatory re-list: a mutation may have changed a counter, a state,
        // or the credential set itself, and no other event announces that.
        requestCredentials();
    });
}

void LiveCardController::activateSigningKey()
{
    if (card.isNull()) {
        Q_EMIT errorOccurred(cardGoneText(), ErrorCode::CardRemoved);
        return;
    }
    AgentOperation* operation = card->activateSigningKey();
    track(operation);
    const OpWatch watch = armWatchdog(operation);
    connect(operation, &AgentOperation::phaseChanged, this, &CardController::pinPhaseChanged); // as managePin

    connect(operation, &AgentOperation::finished, this, [this, operation, watch] {
        watch.dog->stop();
        forget(operation);
        Q_EMIT pinResultReady(operation->pinResult()); // same always-delivered shape as managePin
        operation->deleteLater();
        requestCredentials();
    });
}

void LiveCardController::cancel()
{
    // A copy, because cancelling may terminate an operation synchronously
    // enough to reach forget() and mutate the list under the loop.
    const QList<QPointer<AgentOperation>> pending = inFlight;
    for (const QPointer<AgentOperation>& operation : pending) {
        if (!operation.isNull()) {
            operation->cancel();
        }
    }
}

void LiveCardController::track(AgentOperation* operation)
{
    inFlight.append(operation);
}

void LiveCardController::forget(AgentOperation* operation)
{
    inFlight.removeAll(QPointer<AgentOperation>(operation));
}

} // namespace librecelik::agent
