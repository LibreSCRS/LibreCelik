// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "fake_gateway/fakeagentgateway.h"

#include "fake_gateway/fakecardcontroller.h"
#include "fake_gateway/fakesigncontroller.h"

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <LibreSCRS/AgentClient/SignOptions.h>

#include <QMetaType>

namespace librecelik::test::agent {

void registerFakeMetatypes()
{
    qRegisterMetaType<librecelik::agent::PresenceState>("librecelik::agent::PresenceState");
    qRegisterMetaType<librecelik::agent::SignRowResult>("librecelik::agent::SignRowResult");
    qRegisterMetaType<LibreSCRS::AgentClient::FieldGroup>("LibreSCRS::AgentClient::FieldGroup");
    qRegisterMetaType<QList<LibreSCRS::AgentClient::FieldGroup>>("QList<LibreSCRS::AgentClient::FieldGroup>");
    qRegisterMetaType<QList<LibreSCRS::AgentClient::CertificateInfo>>("QList<LibreSCRS::AgentClient::CertificateInfo>");
    qRegisterMetaType<LibreSCRS::AgentClient::CredentialList>("LibreSCRS::AgentClient::CredentialList");
    qRegisterMetaType<LibreSCRS::AgentClient::PinResult>("LibreSCRS::AgentClient::PinResult");
    qRegisterMetaType<LibreSCRS::AgentClient::OperationPhase>("LibreSCRS::AgentClient::OperationPhase");
}

FakeAgentGateway::FakeAgentGateway(QObject* parent) : librecelik::agent::AgentGateway(parent)
{
    registerFakeMetatypes();
}

void FakeAgentGateway::setPresence(librecelik::agent::PresenceState state)
{
    if (scriptedPresence == state) {
        return;
    }
    scriptedPresence = state;
    Q_EMIT presenceChanged(scriptedPresence);
}

void FakeAgentGateway::removeCard(const QString& id)
{
    for (int index = 0; index < scriptedReaders.size(); ++index) {
        if (scriptedReaders.at(index).cardId != id) {
            continue;
        }
        scriptedReaders[index].hasCard = false;
        scriptedReaders[index].cardId.clear();
    }
    Q_EMIT readersChanged();
    Q_EMIT cardRemoved(id);
}

void FakeAgentGateway::registerCardController(const QString& id, FakeCardController* controller)
{
    cardControllers.insert(id, controller);
}

void FakeAgentGateway::registerSignController(const QString& id, FakeSignController* controller)
{
    signControllers.insert(id, controller);
}

librecelik::agent::PresenceState FakeAgentGateway::presence() const
{
    return scriptedPresence;
}

QList<librecelik::agent::ReaderInfo> FakeAgentGateway::readers() const
{
    return scriptedReaders;
}

QString FakeAgentGateway::agentVersion() const
{
    return scriptedVersion;
}

bool FakeAgentGateway::hasFeature(const QString& token) const
{
    return scriptedFeatures.contains(token);
}

void FakeAgentGateway::refresh()
{
    callLog << QStringLiteral("refresh");
}

librecelik::agent::CardController* FakeAgentGateway::cardController(const QString& cardId)
{
    return cardControllers.value(cardId, nullptr);
}

librecelik::agent::SignController* FakeAgentGateway::signController(const QString& cardId)
{
    return signControllers.value(cardId, nullptr);
}

std::optional<LibreSCRS::AgentClient::LayoutResult> FakeAgentGateway::layoutVisualSignature(const QString& text,
                                                                                            QRectF box) const
{
    Q_UNUSED(text)
    Q_UNUSED(box)
    return scriptedLayout;
}

QByteArray FakeAgentGateway::appearanceFontData() const
{
    return scriptedFontData;
}

QVariantMap FakeAgentGateway::configSnapshot() const
{
    return config;
}

std::optional<LibreSCRS::AgentClient::SyncError> FakeAgentGateway::setConfigValue(const QString& key,
                                                                                  const QVariant& value)
{
    configWrites.append(qMakePair(key, value));
    if (nextRefusal.has_value()) {
        return nextRefusal;
    }
    config.insert(key, value);
    Q_EMIT configChanged(key);
    return std::nullopt;
}

std::optional<LibreSCRS::AgentClient::SyncError> FakeAgentGateway::resetConfigValue(const QString& key)
{
    configResets.append(key);
    if (nextRefusal.has_value()) {
        return nextRefusal;
    }
    config.remove(key);
    Q_EMIT configChanged(key);
    return std::nullopt;
}

void FakeAgentGateway::fetchCertificateDer(const QString& readerId, const QString& certId)
{
    Q_UNUSED(readerId)
    callLog << QStringLiteral("fetchCertificateDer:%1").arg(certId);
    Q_EMIT certificateDerReady(certId, scriptedDer.value(certId));
}

} // namespace librecelik::test::agent
