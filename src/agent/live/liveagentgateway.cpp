// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/live/liveagentgateway.h"

#include "agent/live/livecardcontroller.h"
#include "agent/live/livesigncontroller.h"
#include "agent/opstallwatchdog.h"

#include <LibreSCRS/AgentClient/AgentCard.h>
#include <LibreSCRS/AgentClient/AgentClient.h>
#include <LibreSCRS/AgentClient/AgentOperation.h>
#include <LibreSCRS/AgentClient/AgentReader.h>
#include <LibreSCRS/AgentClient/ClientTimeouts.h>
#include <LibreSCRS/AgentClient/SealedPayload.h> // readBoundedPayload
#include <LibreSCRS/AgentClient/SharedAgentClient.h>

#include <utility> // std::exchange

namespace librecelik::agent {

using LibreSCRS::AgentClient::AgentCard;
using LibreSCRS::AgentClient::AgentOperation;
using LibreSCRS::AgentClient::AgentReader;

LiveAgentGateway::LiveAgentGateway(QObject* parent)
    : AgentGateway(parent), client(LibreSCRS::AgentClient::sharedAgentClient())
{
    // The client's ctor already ran its bounded availability probe and, when
    // reachable, populated the registry — so the roster is valid on the very
    // next line and the initial known-card list is not a guess.
    refreshKnownCards();

    connect(client.get(), &LibreSCRS::AgentClient::AgentClient::availabilityChanged, this,
            &LiveAgentGateway::onAvailabilityChanged);
    connect(client.get(), &LibreSCRS::AgentClient::AgentClient::readersChanged, this,
            &LiveAgentGateway::onReadersChanged);
    connect(client.get(), &LibreSCRS::AgentClient::AgentClient::cardChanged, this, &LiveAgentGateway::onCardChanged);
    // Config1 needs no translation — the key vocabulary is the wire's.
    connect(client.get(), &LibreSCRS::AgentClient::AgentClient::configChanged, this, &AgentGateway::configChanged);
}

LiveAgentGateway::~LiveAgentGateway() = default;

PresenceState LiveAgentGateway::presence() const
{
    return resolvePresence(client->agentInstalled(), client->isAvailable());
}

QList<ReaderInfo> LiveAgentGateway::readers() const
{
    QList<ReaderInfo> rows;
    const QList<AgentReader*> tracked = client->readers(); // already id-sorted
    rows.reserve(tracked.size());
    for (const AgentReader* reader : tracked) {
        rows.append(ReaderInfo{reader->id(), reader->name(), reader->hasCard(), reader->cardId()});
    }
    return rows;
}

QString LiveAgentGateway::agentVersion() const
{
    return client->agentVersion();
}

bool LiveAgentGateway::hasFeature(const QString& token) const
{
    return client->hasFeature(token);
}

void LiveAgentGateway::refresh()
{
    client->refreshDiscovery();
}

CardController* LiveAgentGateway::cardController(const QString& cardId)
{
    // Idempotent by card id: a second controller for the same card would run a
    // second read against a reader that admits one at a time, and would arm a
    // second watchdog over the same operations. Consumers therefore look one
    // up per dialog and get the same object every time.
    if (const auto known = cardControllers.constFind(cardId); known != cardControllers.constEnd() && !known->isNull()) {
        return known->data();
    }
    AgentCard* card = client->card(cardId);
    if (card == nullptr) {
        // An id the roster does not know — a reader id, or a card already
        // removed. There is nothing to control, and inventing a controller for
        // it would only defer the same answer to the first verb.
        return nullptr;
    }
    auto* controller = new LiveCardController(card, client.get(), this);
    cardControllers.insert(cardId, controller);
    return controller;
}

SignController* LiveAgentGateway::signController(const QString& cardId)
{
    if (const auto known = signControllers.constFind(cardId); known != signControllers.constEnd() && !known->isNull()) {
        return known->data();
    }
    AgentCard* card = client->card(cardId);
    if (card == nullptr) {
        return nullptr;
    }
    auto* controller = new LiveSignController(card, client.get(), this);
    signControllers.insert(cardId, controller);
    return controller;
}

void LiveAgentGateway::releaseControllers(const QString& cardId)
{
    // deleteLater, never delete: the removal that brought us here can arrive
    // while a dialog's slot chain still holds this pointer, and the dialogs
    // hold the controller through connections Qt severs on destruction anyway.
    if (auto* controller = cardControllers.take(cardId).data(); controller != nullptr) {
        controller->deleteLater();
    }
    if (auto* controller = signControllers.take(cardId).data(); controller != nullptr) {
        controller->deleteLater();
    }
}

std::optional<LibreSCRS::AgentClient::LayoutResult> LiveAgentGateway::layoutVisualSignature(const QString& text,
                                                                                            QRectF box) const
{
    return client->layoutVisualSignature(text, box);
}

QByteArray LiveAgentGateway::appearanceFontData() const
{
    if (!fontCached) {
        // Engaged-or-empty: a refused gate, an absent agent and a failed read
        // are all "no font" to a preview renderer, which falls back to its own.
        fontData = LibreSCRS::AgentClient::readBoundedPayload(client->appearanceFont()).value_or(QByteArray());
        fontCached = true;
    }
    return fontData;
}

QVariantMap LiveAgentGateway::configSnapshot() const
{
    return client->configSnapshot();
}

std::optional<LibreSCRS::AgentClient::SyncError> LiveAgentGateway::setConfigValue(const QString& key,
                                                                                  const QVariant& value)
{
    return client->setConfigValue(key, value);
}

std::optional<LibreSCRS::AgentClient::SyncError> LiveAgentGateway::resetConfigValue(const QString& key)
{
    return client->resetConfigValue(key);
}

void LiveAgentGateway::fetchCertificateDer(const QString& readerId, const QString& certId)
{
    AgentOperation* operation = client->certificateDer(readerId, certId);
    if (operation == nullptr) {
        // The client documents a non-null operation; answering the
        // export-unavailable shape anyway keeps a viewer from waiting forever
        // on a reply that can no longer arrive.
        Q_EMIT certificateDerReady(certId, QByteArray());
        return;
    }

    // The DER fetch is public data with no consent step, but it is still card
    // I/O and the client enforces no stall bound of its own, so it carries the
    // same caller-side watchdog every controller operation does: parented to
    // the operation, fed by its phase stream, cancelling on expiry. A cancelled
    // fetch answers an EMPTY DER through the very same handler — the viewer
    // gets its answer either way and never waits forever.
    auto* dog = new OpStallWatchdog(LibreSCRS::AgentClient::kLongOperationTimeoutMs, operation);
    connect(operation, &AgentOperation::phaseChanged, dog, &OpStallWatchdog::onPhase);
    connect(dog, &OpStallWatchdog::expired, operation, [operation]() { operation->cancel(); });
    dog->begin();

    connect(operation, &AgentOperation::finished, this, [this, operation, dog, certId]() {
        dog->stop(); // a normal completion must never leave a live timer behind
        Q_EMIT certificateDerReady(certId, operation->certificateDerResult());
        operation->deleteLater();
    });
}

void LiveAgentGateway::onAvailabilityChanged(bool available)
{
    if (available) {
        refreshKnownCards();
    } else {
        // Documented client contract: on agent loss the whole registry is
        // cleared with NO per-card cardChanged. The gateway is therefore the
        // only place that can tell a modal consumer its card is gone.
        const QStringList vanished = std::exchange(knownCards, QStringList{});
        for (const QString& cardId : vanished) {
            releaseControllers(cardId);
            Q_EMIT cardRemoved(cardId);
        }
    }
    // The font is per-connection agent state; a flip in either direction ends
    // its epoch.
    fontData.clear();
    fontCached = false;
    Q_EMIT presenceChanged(presence());
}

void LiveAgentGateway::onReadersChanged()
{
    refreshKnownCards();
    Q_EMIT readersChanged();
}

void LiveAgentGateway::onCardChanged(const QString& objectId)
{
    // The client's cardChanged carries EITHER a card id or a reader id, so a
    // vanished-card verdict is only meaningful for an id we knew as a card:
    // client->card() answers nullptr for every reader id too, and reporting
    // those as removed cards would be a lie every reader property change tells.
    const bool wasKnownCard = knownCards.contains(objectId);
    refreshKnownCards();

    Q_EMIT cardChanged(objectId);
    if (wasKnownCard && client->card(objectId) == nullptr) {
        releaseControllers(objectId);
        Q_EMIT cardRemoved(objectId);
    }
}

void LiveAgentGateway::refreshKnownCards()
{
    knownCards.clear();
    const QList<AgentReader*> tracked = client->readers(); // already id-sorted
    for (const AgentReader* reader : tracked) {
        if (reader->hasCard() && !reader->cardId().isEmpty()) {
            knownCards.append(reader->cardId());
        }
    }
}

} // namespace librecelik::agent
