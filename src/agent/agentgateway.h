// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/AgentClient.h> // AgentState
#include <LibreSCRS/AgentClient/SyncError.h>
#include <LibreSCRS/AgentClient/Types.h>
#include <QObject>
#include <QRectF>
#include <QVariantMap>

#include <expected>
#include <optional>

namespace librecelik::agent {

class CardController;
class SignController;

/// Guided-UX presence states: the client library's agent state, in the
/// words this window's panel speaks.
enum class PresenceState { AgentMissing, AgentUnavailable, Ready };

/// The panel's word for the client's agent state. The client learns that
/// state itself and answers it without asking anyone, so this is safe in any
/// signal handler.
[[nodiscard]] constexpr PresenceState presenceFor(LibreSCRS::AgentClient::AgentState state) noexcept
{
    switch (state) {
    case LibreSCRS::AgentClient::AgentState::Ready:
        return PresenceState::Ready;
    case LibreSCRS::AgentClient::AgentState::Stopped:
        return PresenceState::AgentUnavailable;
    case LibreSCRS::AgentClient::AgentState::Missing:
        break;
    }
    return PresenceState::AgentMissing;
}

struct ReaderInfo
{
    QString id;   ///< opaque reader id
    QString name; ///< friendly name
    bool hasCard = false;
    QString cardId; ///< opaque card id, empty when none
};

class AgentGateway : public QObject
{
    Q_OBJECT
public:
    explicit AgentGateway(QObject* parent = nullptr);
    ~AgentGateway() override;

    [[nodiscard]] virtual PresenceState presence() const = 0;
    [[nodiscard]] virtual QList<ReaderInfo> readers() const = 0;
    [[nodiscard]] virtual QString agentVersion() const = 0;
    [[nodiscard]] virtual bool hasFeature(const QString& token) const = 0;
    virtual void refresh() = 0; // AgentClient::refreshDiscovery passthrough
    /// AgentClient::requestAgentStart passthrough: start the agent if it is
    /// installed but not running. The library knows how on each platform.
    virtual LibreSCRS::AgentClient::AgentStartResult requestAgentStart() = 0;

    /// Per-card controllers, parented to the gateway; nullptr for an id not
    /// in the roster. One live controller per card id (idempotent lookup).
    [[nodiscard]] virtual CardController* cardController(const QString& cardId) = 0;
    [[nodiscard]] virtual SignController* signController(const QString& cardId) = 0;

    // Preview-parity primitives (Phase C).
    [[nodiscard]] virtual std::optional<LibreSCRS::AgentClient::LayoutResult>
    layoutVisualSignature(const QString& text, QRectF box) const = 0;
    [[nodiscard]] virtual QByteArray appearanceFontData() const = 0;

    // Config1 (Phase E).
    [[nodiscard]] virtual QVariantMap configSnapshot() const = 0;
    [[nodiscard]] virtual std::optional<LibreSCRS::AgentClient::SyncError> setConfigValue(const QString& key,
                                                                                          const QVariant& value) = 0;
    /// Config1.Reset — the per-tab "restore defaults" path.
    [[nodiscard]] virtual std::optional<LibreSCRS::AgentClient::SyncError> resetConfigValue(const QString& key) = 0;

    /// Install country-signing (CSCA) trust anchors from a signed ICAO master
    /// list. Answers the accepted anchor state, or the NAMED refusal — in
    /// particular `MasterListReplayed`, the one a person can act on ("the list
    /// you chose is not newer than the one already installed").
    ///
    /// @param masterListFd An OPEN descriptor, BORROWED: the callee duplicates
    ///        it for the wire and never closes yours. A descriptor and not a
    ///        path because the agent is a separate, possibly sandboxed process
    ///        — a name it would have to re-open is a name it may not be able
    ///        to open. One consequence follows from what a descriptor IS: the
    ///        agent's read shares this open file description, so it ADVANCES
    ///        YOUR FILE POSITION. Rewind before reading it yourself afterwards.
    [[nodiscard]] virtual std::expected<LibreSCRS::AgentClient::CscaAnchorState, LibreSCRS::AgentClient::SyncError>
    importCscaMasterList(int masterListFd) = 0;

    // Raw DER fetch for the cert viewer's export path (Phase B).
    virtual void fetchCertificateDer(const QString& readerId, const QString& certId) = 0;

signals:
    void presenceChanged(librecelik::agent::PresenceState state);
    void readersChanged();
    void cardChanged(const QString& objectId);
    /// Emitted for every card that leaves the roster (including all cards at
    /// once on presence loss). Modal-hygiene consumers key off this.
    void cardRemoved(const QString& cardId);
    void configChanged(const QString& key);
    void certificateDerReady(const QString& certId, const QByteArray& der);
};

} // namespace librecelik::agent
