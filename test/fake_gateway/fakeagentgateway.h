// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Scripted `AgentGateway` for offscreen GUI tests.
///
/// Test-only scaffolding: compiled into test targets, never shipped. EVERY
/// `AgentGateway` pure virtual is covered — an abstract fake cannot be
/// instantiated by a test — and every scripted member is a plain public data
/// member (no trailing underscores, house rule).

#pragma once

#include "agent/agentgateway.h"

#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <QVariantMap>

#include <expected>
#include <optional>

namespace librecelik::test::agent {

class FakeCardController;
class FakeSignController;

/// @brief Register the value metatypes a QUEUED delivery of the controller and
///        gateway signals needs (a spy or a receiver on another thread).
///
/// Idempotent, and defined exactly once — in the fake-gateway TU — so the
/// registrations cannot drift apart per fake; every fake's constructor calls
/// it. A direct connection needs none of this, which is why a missing
/// registration shows up as a silently dropped emission rather than a
/// compile error.
void registerFakeMetatypes();

class FakeAgentGateway : public librecelik::agent::AgentGateway
{
    Q_OBJECT
public:
    explicit FakeAgentGateway(QObject* parent = nullptr);

    // ---- scripted state -----------------------------------------------------
    librecelik::agent::PresenceState scriptedPresence = librecelik::agent::PresenceState::AgentMissing;
    QList<librecelik::agent::ReaderInfo> scriptedReaders;
    QStringList scriptedFeatures;
    QString scriptedVersion;
    /// The snapshot `configSnapshot()` returns.
    QVariantMap config;
    /// Engaged => the next `setConfigValue`/`resetConfigValue` refuses with it
    /// (the call is still recorded — a refusal is an attempt, and the
    /// "exactly one attempt, no retry loop" assertions need the record).
    std::optional<LibreSCRS::AgentClient::SyncError> nextRefusal;
    /// The `layoutVisualSignature()` reply; `nullopt` (the default) is the
    /// feature-absent shape a preview degrades on.
    std::optional<LibreSCRS::AgentClient::LayoutResult> scriptedLayout;
    QByteArray scriptedFontData;
    /// Per-certificate DER for `fetchCertificateDer()`; a missing id answers
    /// empty bytes (the export-unavailable shape a viewer tolerates).
    QHash<QString, QByteArray> scriptedDer;
    /// The anchor state `importCscaMasterList()` answers on acceptance.
    LibreSCRS::AgentClient::CscaAnchorState scriptedAnchorState;
    /// Engaged => the next `importCscaMasterList` refuses with it (the call is
    /// still recorded — a refusal is an attempt).
    std::optional<LibreSCRS::AgentClient::SyncError> nextImportRefusal;

    // ---- recorders -----------------------------------------------------------
    QList<QPair<QString, QVariant>> configWrites;
    QStringList configResets;
    /// Descriptors handed to `importCscaMasterList()`, in call order.
    QList<int> importedFds;
    /// What each of those descriptors yielded when READ FROM ITS CURRENT
    /// POSITION — the same thing a real agent's read does, which is why the
    /// sender's own offset moves and a passed name can be told from a passed
    /// descriptor. Never pread(2): that would leave the offset untouched and
    /// make the two indistinguishable.
    QList<QByteArray> importedBytes;
    /// Records verb calls (`refresh`, `fetchCertificateDer:<certId>`, ...).
    QStringList callLog;

    // ---- scripting verbs -----------------------------------------------------
    /// Set the presence, emitting `presenceChanged` only on an actual change.
    void setPresence(librecelik::agent::PresenceState state);
    /// Drop @p id from `scriptedReaders` and emit `cardRemoved` (modal hygiene).
    void removeCard(const QString& id);
    void registerCardController(const QString& id, FakeCardController* controller);
    void registerSignController(const QString& id, FakeSignController* controller);

    // ---- AgentGateway --------------------------------------------------------
    [[nodiscard]] librecelik::agent::PresenceState presence() const override;
    [[nodiscard]] QList<librecelik::agent::ReaderInfo> readers() const override;
    [[nodiscard]] QString agentVersion() const override;
    [[nodiscard]] bool hasFeature(const QString& token) const override;
    void refresh() override;

    [[nodiscard]] librecelik::agent::CardController* cardController(const QString& cardId) override;
    [[nodiscard]] librecelik::agent::SignController* signController(const QString& cardId) override;

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
    QHash<QString, FakeCardController*> cardControllers;
    QHash<QString, FakeSignController*> signControllers;
};

} // namespace librecelik::test::agent
