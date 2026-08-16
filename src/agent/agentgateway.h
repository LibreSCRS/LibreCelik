// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/SyncError.h>
#include <LibreSCRS/AgentClient/Types.h>
#include <QObject>
#include <QRectF>
#include <QVariantMap>
#include <optional>

namespace librecelik::agent {

class CardController;
class SignController;

/// Guided-UX presence states (spec §5.4). Detection is ClientQt's:
/// agentInstalled() is activation-aware (D-Bus ListActivatableNames).
enum class PresenceState { AgentMissing, AgentUnavailable, Ready };

[[nodiscard]] constexpr PresenceState resolvePresence(bool installed, bool available) noexcept
{
    if (available)
        return PresenceState::Ready;
    return installed ? PresenceState::AgentUnavailable : PresenceState::AgentMissing;
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
    /// Config1.Reset — the per-tab "restore defaults" path (Task 29).
    [[nodiscard]] virtual std::optional<LibreSCRS::AgentClient::SyncError> resetConfigValue(const QString& key) = 0;

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
