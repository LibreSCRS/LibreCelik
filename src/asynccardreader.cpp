// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "asynccardreader.h"

#include <smartcard/pcsc_connection.h>
#include <smartcard/secure_buffer.h>

#include <openssl/crypto.h>

#include <QMap>
#include <QMetaObject>
#include <QMetaType>
#include <QPointer>

namespace {

// Alias the shared PIN-string scrubber (defined in smartcard/secure_buffer.h)
// under the name this translation unit historically used.
using StdStringScrubber = ::smartcard::PinStringScrubber;

/// Materialize a std::string from a SecureBuffer for passing to CardPlugin
/// APIs that take `const std::string&`. The returned string is a temporary
/// uncleansed copy; wrap it with StdStringScrubber in the caller's scope.
std::string toStdString(const smartcard::SecureBuffer& buf)
{
    return std::string(reinterpret_cast<const char*>(buf.data()), buf.size());
}

} // namespace

AsyncCardReader::AsyncCardReader(std::vector<plugin::CardPlugin*> candidates,
                                 std::vector<plugin::CardPlugin*> allPlugins,
                                 std::unique_ptr<smartcard::PCSCConnection> conn, QObject* parent)
    : QObject(parent), candidates(std::move(candidates)), allPlugins(std::move(allPlugins)), conn(std::move(conn))
{
    qRegisterMetaType<plugin::CardData>("plugin::CardData");
    qRegisterMetaType<plugin::CardFieldGroup>("plugin::CardFieldGroup");
    qRegisterMetaType<std::vector<plugin::CertificateData>>("std::vector<plugin::CertificateData>");
}

AsyncCardReader::~AsyncCardReader()
{
    cancel();
}

void AsyncCardReader::cancel()
{
    stopRequested = true;
    if (conn)
        conn->cancel();
    waitForPendingAsync();
}

void AsyncCardReader::initiateCancel()
{
    stopRequested = true;
    if (conn)
        conn->cancel();
}

void AsyncCardReader::waitForPendingAsync()
{
    if (futureData.valid())
        futureData.wait();
    if (futurePKI.valid())
        futurePKI.wait();
}

void AsyncCardReader::clearPluginCredentials()
{
    if (activePlugin && conn)
        activePlugin->clearCredentials(*conn);
}

plugin::CardPlugin* AsyncCardReader::currentPlugin() const
{
    return activePlugin;
}

bool AsyncCardReader::hasPKI() const
{
    return (activePlugin && activePlugin->supportsPKI()) || (pkiPlugin && pkiPlugin->supportsPKI());
}

void AsyncCardReader::requestData()
{
    waitForPendingAsync();
    futureData = {};

    stopRequested = false;
    certsAlreadyQueued = false;
    emit readingStarted();

    QPointer<AsyncCardReader> self = this;
    futureData = std::async(std::launch::async, [this, self]() {
        if (!conn) {
            QMetaObject::invokeMethod(
                self,
                [this, self]() {
                    if (!self)
                        return;
                    emit errorOccurred(qtTrId("lc-error-no-connection"));
                    emit readingFinished();
                },
                Qt::QueuedConnection);
            return;
        }

        // Note: if a candidate emits groups via onGroup then throws, those groups
        // are already queued to the GUI thread. The user may briefly see partial UI
        // from the wrong plugin. When the correct plugin succeeds, cardDataReady
        // replaces the widget. Acceptable tradeoff for v1 (avoids double card read).
        for (auto* candidate : candidates) {
            if (stopRequested)
                return;
            try {
                auto callback = [self](const std::string& cardType, const plugin::CardFieldGroup& group) {
                    if (!self)
                        return;
                    auto ct = QString::fromStdString(cardType);
                    QMetaObject::invokeMethod(
                        self,
                        [self, ct, group]() {
                            if (!self)
                                return;
                            emit self->cardGroupReady(ct, group);
                        },
                        Qt::QueuedConnection);
                };

                auto data = candidate->readCardStreaming(*conn, callback);
                QMetaObject::invokeMethod(
                    self,
                    [this, self, data = std::move(data), candidate]() {
                        if (!self)
                            return;
                        activePlugin = candidate;
                        pkiPlugin = nullptr;
                        if (!activePlugin->supportsPKI()) {
                            for (auto* c : candidates) {
                                if (c != activePlugin && c->supportsPKI()) {
                                    pkiPlugin = c;
                                    break;
                                }
                            }
                        }
                        emit cardDataReady(data);
                        emit readingFinished();
                    },
                    Qt::QueuedConnection);
                return;
            } catch (const std::exception&) {
                // Try next candidate
            }
        }

        // All candidates failed
        QMetaObject::invokeMethod(
            self,
            [this, self]() {
                if (!self)
                    return;
                emit errorOccurred(qtTrId("lc-error-no-plugin"));
                emit readingFinished();
            },
            Qt::QueuedConnection);
    });
}

void AsyncCardReader::requestCertificates()
{
    // requestDataWithCredentials already read certs inline and queued
    // certificatesReady — skip redundant read.
    if (certsAlreadyQueued)
        return;

    auto* pki = pkiPlugin ? pkiPlugin : activePlugin;
    if (!pki || !pki->supportsPKI())
        return;

    waitForPendingAsync();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki]() {
        if (stopRequested)
            return;
        try {
            // Read token info first
            auto tokenInfo = pki->readTokenInfo(*conn);
            if (!tokenInfo.fields.empty()) {
                QMetaObject::invokeMethod(
                    self,
                    [self, tokenInfo = std::move(tokenInfo)]() {
                        if (!self)
                            return;
                        emit self->tokenInfoReady(tokenInfo);
                    },
                    Qt::QueuedConnection);
            }

            if (stopRequested)
                return;

            auto certs = pki->readCertificates(*conn);
            QMetaObject::invokeMethod(
                self,
                [self, certs = std::move(certs)]() {
                    if (!self)
                        return;
                    emit self->certificatesReady(certs);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestPINTriesLeft(uint8_t pinReference)
{
    auto* pki = pkiPlugin ? pkiPlugin : activePlugin;
    if (!pki || !pki->supportsPKI())
        return;

    waitForPendingAsync();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinReference]() {
        if (stopRequested)
            return;
        try {
            auto pins = pki->getPINList(*conn);
            int tries = -1;
            for (const auto& p : pins) {
                if (p.reference == pinReference) {
                    tries = p.triesLeft;
                    break;
                }
            }
            if (tries == -1 && pins.empty()) {
                // Fallback to single-PIN API
                tries = pki->getPINTriesLeft(*conn);
            }
            QMetaObject::invokeMethod(
                self,
                [self, tries]() {
                    if (!self)
                        return;
                    emit self->pinStatusReady(tries, tries == 0);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestPINList()
{
    auto* pki = pkiPlugin ? pkiPlugin : activePlugin;
    if (!pki || !pki->supportsPKI())
        return;

    waitForPendingAsync();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki]() {
        if (stopRequested)
            return;
        try {
            auto pins = pki->getPINList(*conn);
            QMetaObject::invokeMethod(
                self,
                [self, pins = std::move(pins)]() {
                    if (!self)
                        return;
                    emit self->pinListReady(pins);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestChangePIN(uint8_t pinReference, const QString& oldPin, const QString& newPin)
{
    auto* pki = pkiPlugin ? pkiPlugin : activePlugin;
    if (!pki || !pki->supportsPKI())
        return;

    waitForPendingAsync();

    auto oldPinStdTmp = oldPin.toStdString();
    smartcard::SecureBuffer oldPinBuffer(oldPinStdTmp);
    StdStringScrubber oldPinTmpScrubber{oldPinStdTmp};

    auto newPinStdTmp = newPin.toStdString();
    smartcard::SecureBuffer newPinBuffer(newPinStdTmp);
    StdStringScrubber newPinTmpScrubber{newPinStdTmp};

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinReference, oldPinBuffer = std::move(oldPinBuffer),
                                                newPinBuffer = std::move(newPinBuffer)]() mutable {
        if (stopRequested)
            return;
        try {
            plugin::PINResult result;
            {
                std::string oldPinStd = toStdString(oldPinBuffer);
                StdStringScrubber oldScrubber{oldPinStd};
                std::string newPinStd = toStdString(newPinBuffer);
                StdStringScrubber newScrubber{newPinStd};

                result = pki->changePIN(*conn, pinReference, oldPinStd, newPinStd);
                if (!result.success && result.retriesLeft == -1 && !result.blocked) {
                    result = pki->changePIN(*conn, oldPinStd, newPinStd);
                }
            }

            QMetaObject::invokeMethod(
                self,
                [self, result]() {
                    if (!self)
                        return;
                    emit self->pinChangeResult(result.success, result.retriesLeft,
                                               result.success ? QString() : qtTrId("lc-changepin-failed"));
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->pinChangeResult(false, -1, msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestVerifyPIN(const QString& pin)
{
    auto* pki = pkiPlugin ? pkiPlugin : activePlugin;
    if (!pki || !pki->supportsPKI())
        return;

    waitForPendingAsync();

    auto pinStdTmp = pin.toStdString();
    smartcard::SecureBuffer pinBuffer(pinStdTmp);
    StdStringScrubber pinTmpScrubber{pinStdTmp};

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinBuffer = std::move(pinBuffer)]() mutable {
        if (stopRequested)
            return;
        try {
            plugin::PINResult result;
            {
                std::string pinStd = toStdString(pinBuffer);
                StdStringScrubber scrubber{pinStd};
                result = pki->verifyPIN(*conn, pinStd);
            }
            QMetaObject::invokeMethod(
                self,
                [self, result]() {
                    if (!self)
                        return;
                    emit self->pinVerifyResult(result.success, result.retriesLeft);
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self,
                [self, msg = QString::fromStdString(e.what())]() {
                    if (!self)
                        return;
                    emit self->errorOccurred(msg);
                },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestDataWithCredentials(const QMap<QString, QString>& credentials)
{
    if (!activePlugin)
        return;

    waitForPendingAsync();

    stopRequested = false;

    for (auto it = credentials.constBegin(); it != credentials.constEnd(); ++it) {
        activePlugin->setCredentials(*conn, it.key().toStdString(), it.value().toStdString());
    }

    futureData = {};

    emit readingStarted();

    QPointer<AsyncCardReader> self2 = this;
    futureData = std::async(std::launch::async, [this, self2]() {
        if (stopRequested)
            return;
        try {
            auto callback = [self2](const std::string& cardType, const plugin::CardFieldGroup& group) {
                if (!self2)
                    return;
                auto ct = QString::fromStdString(cardType);
                QMetaObject::invokeMethod(
                    self2,
                    [self2, ct, group]() {
                        if (!self2)
                            return;
                        emit self2->cardGroupReady(ct, group);
                    },
                    Qt::QueuedConnection);
            };

            auto data = activePlugin->readCardStreaming(*conn, callback);

            // PKI fallback — SM filter stays active (installed by eMRTD plugin
            // after PACE). Re-probe for PKI plugins that require authentication
            // (e.g., PKCS#15 on contactless needs PACE before AID SELECT).
            if (!pkiPlugin && !activePlugin->supportsPKI()) {
                for (auto* p : allPlugins) {
                    if (p == activePlugin)
                        continue;
                    try {
                        if (p->supportsPKI() && p->canHandleConnection(*conn)) {
                            pkiPlugin = p;
                            break;
                        }
                    } catch (...) {
                    }
                }
            }

            std::vector<plugin::CertificateData> certs;
            plugin::CardFieldGroup tokenInfo;
            auto* pki = pkiPlugin ? pkiPlugin : (activePlugin->supportsPKI() ? activePlugin : nullptr);
            if (pki && pki->supportsPKI()) {
                try {
                    tokenInfo = pki->readTokenInfo(*conn);
                } catch (...) {
                }
                try {
                    certs = pki->readCertificates(*conn);
                } catch (...) {
                    // Certificate reading failed (e.g. CL without PACE/SM).
                    // Clear pkiPlugin so hasPKI() returns false — avoids
                    // showing an empty token section and spurious errors.
                    if (pkiPlugin == pki)
                        pkiPlugin = nullptr;
                }
            }

            QMetaObject::invokeMethod(
                self2,
                [this, self2, data = std::move(data), certs = std::move(certs), tokenInfo = std::move(tokenInfo)]() {
                    if (!self2)
                        return;
                    certsAlreadyQueued = !certs.empty();
                    emit cardDataReady(data);
                    emit readingFinished();
                    if (!tokenInfo.fields.empty())
                        emit tokenInfoReady(tokenInfo);
                    if (!certs.empty()) {
                        QMetaObject::invokeMethod(
                            self2,
                            [this, self2, certs]() {
                                if (!self2)
                                    return;
                                emit certificatesReady(certs);
                            },
                            Qt::QueuedConnection);
                    }
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                self2,
                [this, self2, msg = QString::fromStdString(e.what())]() {
                    if (!self2)
                        return;
                    emit errorOccurred(msg);
                    emit readingFinished();
                },
                Qt::QueuedConnection);
        }
    });
}
