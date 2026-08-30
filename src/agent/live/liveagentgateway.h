// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief The production `AgentGateway`: the LC service layer over the
///        process-wide ClientQt connection.
///
/// Owns nothing the client already owns — no transport, no registry, no
/// polling. What it adds is the LC-facing vocabulary: guided-UX presence
/// states, plain `ReaderInfo` rows, and a per-card removal signal the client
/// deliberately does not emit on agent loss (it clears the whole registry at
/// once), which modal consumers key off.

#pragma once

#include "agent/agentgateway.h"

#include <LibreSCRS/AgentClient/SyncError.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPointer>
#include <QRectF>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <expected>
#include <memory>
#include <optional>

namespace LibreSCRS::AgentClient {
class AgentClient;
}

namespace librecelik::agent {

class LiveCardController;
class LiveSignController;

class LiveAgentGateway final : public AgentGateway
{
    Q_OBJECT
public:
    explicit LiveAgentGateway(QObject* parent = nullptr);
    ~LiveAgentGateway() override;

    [[nodiscard]] PresenceState presence() const override;
    [[nodiscard]] QList<ReaderInfo> readers() const override;
    [[nodiscard]] QString agentVersion() const override;
    [[nodiscard]] bool hasFeature(const QString& token) const override;
    void refresh() override;

    [[nodiscard]] CardController* cardController(const QString& cardId) override;
    [[nodiscard]] SignController* signController(const QString& cardId) override;

    [[nodiscard]] std::optional<LibreSCRS::AgentClient::LayoutResult> layoutVisualSignature(const QString& text,
                                                                                            QRectF box) const override;
    [[nodiscard]] QByteArray appearanceFontData() const override;

    [[nodiscard]] QVariantMap configSnapshot() const override;
    [[nodiscard]] std::optional<LibreSCRS::AgentClient::SyncError> setConfigValue(const QString& key,
                                                                                  const QVariant& value) override;
    [[nodiscard]] std::optional<LibreSCRS::AgentClient::SyncError> resetConfigValue(const QString& key) override;
    [[nodiscard]] std::expected<LibreSCRS::AgentClient::CscaAnchorState, LibreSCRS::AgentClient::SyncError>
    importCscaMasterList(int masterListFd) override;

    void fetchCertificateDer(const QString& readerId, const QString& certId) override;

private:
    void onAvailabilityChanged(bool available);
    void onReadersChanged();
    void onCardChanged(const QString& objectId);
    /// Re-read the roster's card ids. Kept in the client's own deterministic
    /// id-sorted order, so the removal storm on agent loss is reproducible.
    void refreshKnownCards();
    /// Drop the controllers of a card that is gone. Deferred deletion, never
    /// synchronous: the removal can arrive while a dialog's slot chain still
    /// holds the pointer.
    void releaseControllers(const QString& cardId);

    std::shared_ptr<LibreSCRS::AgentClient::AgentClient> client;

    /// One live controller per card id, minted on first lookup and kept until
    /// the card goes away. Held weakly so a released entry that outlives its
    /// `deleteLater()` can never be handed out again.
    QHash<QString, QPointer<LiveCardController>> cardControllers;
    QHash<QString, QPointer<LiveSignController>> signControllers;

    /// Card ids last seen in the roster — the memory the client does not keep
    /// for us: on `availabilityChanged(false)` the registry is already empty,
    /// so this list is the only source of the ids to report as removed.
    QStringList knownCards;

    /// The embedded appearance font, cached per availability epoch (the
    /// bytes are per-connection agent state; a reconnect may serve others).
    /// Mutable because the accessor is const on the abstract seam.
    mutable QByteArray fontData;
    mutable bool fontCached = false;
};

} // namespace librecelik::agent
