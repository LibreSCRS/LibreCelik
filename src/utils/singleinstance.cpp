// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/singleinstance.h"

#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QStandardPaths>

#include <utility>

namespace librecelik::utils {

namespace {

/// The one message a later start sends. Anything else on the socket is not a
/// LibreCelik asking and is ignored.
constexpr QByteArrayView kShowRequest{"show\n"};
/// How long a later start waits for the running instance. A local socket
/// answers at once; the bound only keeps a wedged instance from wedging the
/// start that asked it too.
constexpr int kAnswerTimeoutMs = 1000;

} // namespace

SingleInstance::SingleInstance(QString socketName, QObject* parent) : QObject(parent), name(std::move(socketName)) {}

SingleInstance::~SingleInstance() = default;

QString SingleInstance::defaultName()
{
    QString dir = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (dir.isEmpty())
        dir = QDir::tempPath();
    return QDir(dir).filePath(QStringLiteral("librecelik.socket"));
}

bool SingleInstance::claim()
{
    if (askRunningInstance())
        return false;
    if (listen())
        return true;
    // Another start bound the name between the two steps above: it is the
    // instance now, so ask it. If nothing answers, the name is a socket file
    // left by an instance that did not exit cleanly; it is reclaimed.
    if (askRunningInstance())
        return false;
    QLocalServer::removeServer(name);
    // Becoming the instance is best effort: a window with no single-instance
    // guard is still a working window.
    (void)listen();
    return true;
}

bool SingleInstance::askRunningInstance() const
{
    QLocalSocket socket;
    socket.connectToServer(name);
    if (!socket.waitForConnected(kAnswerTimeoutMs))
        return false;
    socket.write(kShowRequest.data(), kShowRequest.size());
    socket.waitForBytesWritten(kAnswerTimeoutMs);
    socket.disconnectFromServer();
    return true;
}

bool SingleInstance::listen()
{
    auto* candidate = new QLocalServer(this);
    // Only this user may ask the instance anything.
    candidate->setSocketOptions(QLocalServer::UserAccessOption);
    if (!candidate->listen(name)) {
        delete candidate;
        return false;
    }
    server = candidate;
    connect(server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket* peer = server->nextPendingConnection()) {
            const auto readRequest = [this, peer] {
                if (peer->canReadLine() && peer->readLine() == kShowRequest.toByteArray())
                    emit activationRequested();
            };
            connect(peer, &QLocalSocket::readyRead, this, readRequest);
            connect(peer, &QLocalSocket::disconnected, peer, &QObject::deleteLater);
            readRequest(); // the request may already be buffered
        }
    });
    return true;
}

} // namespace librecelik::utils
