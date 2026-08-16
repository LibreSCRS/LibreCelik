// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The production `CardController`: one card's flows over the agent
///        client, in the LC vocabulary the GUI consumes.
///
/// Adds three things the client deliberately does not provide. A CALLER-SIDE
/// stall bound per operation (the library has none — see `OpStallWatchdog`).
/// The photo MERGE: the wire delivers a portrait through a second operation,
/// and every LC surface consumes it as a field of the identity model, so the
/// read is not finished until the two are one model. And the FEATURE GATE on
/// the optional sections: against an agent predating token info / credentials
/// the client refuses those verbs LOUDLY, which would surface as an error on
/// every insertion for surfaces LC should simply not show.

#pragma once

#include "agent/cardcontroller.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QList>
#include <QPointer>
#include <QString>

#include <cstdint>

namespace LibreSCRS::AgentClient {
class AgentCard;
class AgentClient;
class AgentOperation;
} // namespace LibreSCRS::AgentClient

namespace librecelik::agent {

class LiveCardController final : public CardController
{
    Q_OBJECT
public:
    LiveCardController(LibreSCRS::AgentClient::AgentCard* card, LibreSCRS::AgentClient::AgentClient* client,
                       QObject* parent = nullptr);
    ~LiveCardController() override;

    [[nodiscard]] QString cardId() const override;
    [[nodiscard]] QString readerId() const override;
    [[nodiscard]] QString cardType() const override;
    [[nodiscard]] QString atrHex() const override;
    [[nodiscard]] std::uint32_t capabilityBits() const override;
    [[nodiscard]] LibreSCRS::AgentClient::PreReadAuth preReadAuth() const override;

    void startRead() override;
    void requestCertificates() override;
    void requestTokenInfo() override;
    void requestCredentials() override;
    void managePin(const QString& pinId, LibreSCRS::AgentClient::PinVerb verb,
                   const LibreSCRS::AgentClient::ManagePinOptions& options = {}) override;
    void activateSigningKey() override;
    void cancel() override;

private:
    /// Remember @p operation so `cancel()` can reach it; forgotten again when
    /// it terminates.
    void track(LibreSCRS::AgentClient::AgentOperation* operation);
    void forget(LibreSCRS::AgentClient::AgentOperation* operation);

    /// The identity read's second half: fetch the portrait (when the card
    /// carries identity data at all), merge it into @p groups, and finish.
    void chainPhoto(QList<LibreSCRS::AgentClient::FieldGroup> groups);
    /// Emit the completed read. @p photoMerged streams the merged photo group
    /// as a FINAL `groupReady` first — the progressive path cannot carry it
    /// otherwise (spec §5.4 merge-before-dispatch, D10).
    void finishRead(const QList<LibreSCRS::AgentClient::FieldGroup>& groups, bool photoMerged);
    /// The read-path failure shape: one localized line, then the terminal.
    void failRead(const QString& message);

    /// The card is DELETED by the client on removal while queued slots may
    /// still run, so it is held weakly and null-checked at every use — never a
    /// raw cached pointer.
    QPointer<LibreSCRS::AgentClient::AgentCard> card;
    /// Owned by the process-wide shared client the gateway holds alive, and
    /// the gateway parents this controller, so it cannot dangle here. Used for
    /// the feature membership checks only.
    LibreSCRS::AgentClient::AgentClient* client = nullptr;

    /// In-flight operations, weakly held: the client parents them to the card
    /// and sweeps them when the card vanishes.
    QList<QPointer<LibreSCRS::AgentClient::AgentOperation>> inFlight;
};

} // namespace librecelik::agent
