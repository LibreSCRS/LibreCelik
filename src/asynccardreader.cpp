// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "asynccardreader.h"

#include <smartcard/pcsc_connection.h>

#include <QMap>
#include <QMetaObject>
#include <QMetaType>
#include <QPointer>

AsyncCardReader::AsyncCardReader(std::vector<plugin::CardPlugin*> candidates,
                                 std::unique_ptr<smartcard::PCSCConnection> conn, QObject* parent)
    : QObject(parent), candidates(std::move(candidates)), conn(std::move(conn))
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
    waitForPendingAsync();
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

void AsyncCardReader::requestPINTriesLeft()
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
            int tries = pki->getPINTriesLeft(*conn);
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

void AsyncCardReader::requestChangePIN(const QString& oldPin, const QString& newPin)
{
    auto* pki = pkiPlugin ? pkiPlugin : activePlugin;
    if (!pki || !pki->supportsPKI())
        return;

    waitForPendingAsync();

    auto oldPinStd = oldPin.toStdString();
    auto newPinStd = newPin.toStdString();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, oldPinStd, newPinStd]() {
        if (stopRequested)
            return;
        try {
            auto result = pki->changePIN(*conn, oldPinStd, newPinStd);
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
            if (!pins.empty()) {
                QMetaObject::invokeMethod(
                    self,
                    [self, pins = std::move(pins)]() {
                        if (!self)
                            return;
                        emit self->pinListReady(pins);
                    },
                    Qt::QueuedConnection);
            } else {
                int tries = pki->getPINTriesLeft(*conn);
                QMetaObject::invokeMethod(
                    self,
                    [self, tries]() {
                        if (!self)
                            return;
                        emit self->pinStatusReady(tries, tries == 0);
                    },
                    Qt::QueuedConnection);
            }
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

    auto oldPinStd = oldPin.toStdString();
    auto newPinStd = newPin.toStdString();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinReference, oldPinStd, newPinStd]() {
        if (stopRequested)
            return;
        try {
            auto result = pki->changePIN(*conn, pinReference, oldPinStd, newPinStd);
            if (!result.success && result.retriesLeft == -1 && !result.blocked) {
                result = pki->changePIN(*conn, oldPinStd, newPinStd);
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

    auto pinStd = pin.toStdString();

    QPointer<AsyncCardReader> self = this;
    futurePKI = std::async(std::launch::async, [this, self, pki, pinStd]() {
        if (stopRequested)
            return;
        try {
            auto result = pki->verifyPIN(*conn, pinStd);
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

            // Clear SM filter before PKI fallback — eMRTD SM wrapping
            // may interfere with VERIFY and PKCS#15 operations on contact.
            // Safe: this runs on the sole worker thread using conn (waitForPendingAsync
            // ensures no other thread is accessing conn concurrently).
            conn->clearTransmitFilter();

            // PKI fallback — pkiPlugin was discovered during Phase 1 requestData().
            // Read certificates inline while the connection is still live.
            // PIN tries are NOT read here — connectPKISignals chains
            // certificatesReady → requestPINTriesLeft to avoid double-read.
            std::vector<plugin::CertificateData> certs;
            auto* pki = pkiPlugin ? pkiPlugin : (activePlugin->supportsPKI() ? activePlugin : nullptr);
            if (pki && pki->supportsPKI()) {
                try {
                    certs = pki->readCertificates(*conn);
                } catch (...) {
                }
            }

            QMetaObject::invokeMethod(
                self2,
                [this, self2, data = std::move(data), certs = std::move(certs)]() {
                    if (!self2)
                        return;
                    certsAlreadyQueued = !certs.empty();
                    emit cardDataReady(data);
                    emit readingFinished();
                    // Emit certificatesReady in a separate queued invocation so that
                    // cardDataReady handlers (which may themselves be QueuedConnection)
                    // have a chance to call connectPKISignals before the signal fires.
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
