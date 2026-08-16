// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/AgentCapabilities.h> // PreReadAuth, Cap::, UiState
#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/SignOptions.h> // PinVerb, ManagePinOptions
#include <LibreSCRS/AgentClient/Types.h>
#include <QObject>

namespace librecelik::agent {

class CardController : public QObject
{
    Q_OBJECT
public:
    explicit CardController(QObject* parent = nullptr);
    ~CardController() override;

    [[nodiscard]] virtual QString cardId() const = 0;
    [[nodiscard]] virtual QString readerId() const = 0;
    [[nodiscard]] virtual QString cardType() const = 0; // G10, empty until resolved
    [[nodiscard]] virtual QString atrHex() const = 0;   // uppercase, no separators
    [[nodiscard]] virtual std::uint32_t capabilityBits() const = 0;
    [[nodiscard]] virtual LibreSCRS::AgentClient::PreReadAuth preReadAuth() const = 0;

    /// ReadIdentity (streams groupReady) then GetPhoto; the controller merges
    /// the photo into the final field model before identityReady.
    virtual void startRead() = 0;
    virtual void requestCertificates() = 0; // + warmCertificates opportunistically
    virtual void requestTokenInfo() = 0;
    virtual void requestCredentials() = 0;
    virtual void managePin(const QString& pinId, LibreSCRS::AgentClient::PinVerb verb,
                           const LibreSCRS::AgentClient::ManagePinOptions& options = {}) = 0;
    virtual void activateSigningKey() = 0;
    virtual void cancel() = 0;

signals:
    void readingStarted();
    void groupReady(const LibreSCRS::AgentClient::FieldGroup& group);
    void identityReady(const QList<LibreSCRS::AgentClient::FieldGroup>& groups);
    void certificatesReady(const QList<LibreSCRS::AgentClient::CertificateInfo>& certs);
    void tokenInfoReady(const LibreSCRS::AgentClient::FieldGroup& tokenGroup);
    void credentialsReady(const LibreSCRS::AgentClient::CredentialList& credentials);
    void pinResultReady(const LibreSCRS::AgentClient::PinResult& result);
    void cardTypeResolved(const QString& cardType);
    void errorOccurred(const QString& localizedMessage); // via errortext
    void readingFinished();
};

} // namespace librecelik::agent
