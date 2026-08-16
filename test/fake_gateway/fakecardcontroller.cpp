// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "fake_gateway/fakecardcontroller.h"

#include "fake_gateway/fakeagentgateway.h"

#include <utility>

namespace librecelik::test::agent {

FakeCardController::FakeCardController(QObject* parent) : librecelik::agent::CardController(parent)
{
    registerFakeMetatypes();
}

QString FakeCardController::cardId() const
{
    return scriptedCardId;
}

QString FakeCardController::readerId() const
{
    return scriptedReaderId;
}

QString FakeCardController::cardType() const
{
    return scriptedCardType;
}

QString FakeCardController::atrHex() const
{
    return scriptedAtr;
}

std::uint32_t FakeCardController::capabilityBits() const
{
    return scriptedCaps;
}

LibreSCRS::AgentClient::PreReadAuth FakeCardController::preReadAuth() const
{
    return scriptedPreAuth;
}

void FakeCardController::startRead()
{
    callLog << QStringLiteral("startRead");
    Q_EMIT readingStarted();
    if (failNextRead) {
        // Mirrors the live controller's failed-read shape (errorOccurred then
        // readingFinished, no identityReady). One-shot: a fake that stayed
        // failed would make a later success in the same case impossible to
        // script.
        failNextRead = false;
        Q_EMIT errorOccurred(scriptedError);
        Q_EMIT readingFinished();
        return;
    }
    if (!scriptedCardType.isEmpty()) {
        Q_EMIT cardTypeResolved(scriptedCardType);
    }
    QList<LibreSCRS::AgentClient::FieldGroup> groups = scriptedGroups;
    for (const LibreSCRS::AgentClient::FieldGroup& group : std::as_const(scriptedGroups)) {
        Q_EMIT groupReady(group);
    }
    // The Task-8 merge emission: the photo group is the FINAL groupReady and
    // rides along in the identityReady model.
    if (scriptedPhotoGroup.has_value()) {
        groups.append(*scriptedPhotoGroup);
        Q_EMIT groupReady(*scriptedPhotoGroup);
    }
    Q_EMIT identityReady(groups);
    Q_EMIT readingFinished();
}

void FakeCardController::requestCertificates()
{
    callLog << QStringLiteral("requestCertificates");
    Q_EMIT certificatesReady(scriptedCerts);
}

void FakeCardController::requestTokenInfo()
{
    callLog << QStringLiteral("requestTokenInfo");
    Q_EMIT tokenInfoReady(scriptedTokenInfo);
}

void FakeCardController::requestCredentials()
{
    callLog << QStringLiteral("requestCredentials");
    Q_EMIT credentialsReady(scriptedCredentials);
}

void FakeCardController::managePin(const QString& pinId, LibreSCRS::AgentClient::PinVerb verb,
                                   const LibreSCRS::AgentClient::ManagePinOptions& options)
{
    Q_UNUSED(verb)
    Q_UNUSED(options)
    callLog << QStringLiteral("managePin:%1").arg(pinId);
    replayPinPhases();
    Q_EMIT pinResultReady(scriptedPinResult);
}

void FakeCardController::activateSigningKey()
{
    callLog << QStringLiteral("activateSigningKey");
    replayPinPhases();
    Q_EMIT pinResultReady(scriptedPinResult);
}

void FakeCardController::cancel()
{
    callLog << QStringLiteral("cancel");
}

void FakeCardController::replayPinPhases()
{
    // Before the result, as the live controller's stream arrives: what the
    // mutation is doing is said while it is still doing it.
    for (const LibreSCRS::AgentClient::OperationPhase phase : std::as_const(scriptedPinPhases)) {
        Q_EMIT pinPhaseChanged(phase, 0.0);
    }
}

} // namespace librecelik::test::agent
