// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Scripted `CardController` for offscreen GUI tests.
///
/// Test-only scaffolding: compiled into test targets, never shipped. Every
/// verb appends its own name to `callLog` and replays the scripted reply
/// synchronously, in the SAME order the live controller guarantees — the
/// ordering itself is asserted through `controllercontract.h`, which both this
/// fake and the live controller are pinned against.

#pragma once

#include "agent/cardcontroller.h"

#include <QList>
#include <QString>
#include <QStringList>

#include <cstdint>
#include <optional>

namespace librecelik::test::agent {

class FakeCardController : public librecelik::agent::CardController
{
    Q_OBJECT
public:
    explicit FakeCardController(QObject* parent = nullptr);

    // ---- scripted state -----------------------------------------------------
    QString scriptedCardId;
    QString scriptedReaderId;
    QString scriptedCardType;
    QString scriptedAtr;
    std::uint32_t scriptedCaps = 0;
    LibreSCRS::AgentClient::PreReadAuth scriptedPreAuth = LibreSCRS::AgentClient::PreReadAuth::None;
    QList<LibreSCRS::AgentClient::FieldGroup> scriptedGroups;
    /// Engaged script => startRead() replays the Task-8 merge emission: a FINAL
    /// groupReady with this group (key "photo") BEFORE identityReady, and
    /// identityReady carries it as the last group.
    std::optional<LibreSCRS::AgentClient::FieldGroup> scriptedPhotoGroup;
    QList<LibreSCRS::AgentClient::CertificateInfo> scriptedCerts;
    LibreSCRS::AgentClient::FieldGroup scriptedTokenInfo;
    LibreSCRS::AgentClient::CredentialList scriptedCredentials;
    LibreSCRS::AgentClient::PinResult scriptedPinResult;
    /// Records verb calls for assertions.
    QStringList callLog;

    // ---- CardController ------------------------------------------------------
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
};

} // namespace librecelik::test::agent
