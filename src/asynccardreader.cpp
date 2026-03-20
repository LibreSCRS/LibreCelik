// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "asynccardreader.h"

#include <smartcard/pcsc_connection.h>

#include <QMetaObject>
#include <QMetaType>

AsyncCardReader::AsyncCardReader(std::vector<plugin::CardPlugin*> candidates,
                                 std::unique_ptr<smartcard::PCSCConnection> conn, QObject* parent)
    : QObject(parent), candidates(std::move(candidates)), conn(std::move(conn))
{
    qRegisterMetaType<plugin::CardData>("plugin::CardData");
    qRegisterMetaType<std::vector<plugin::CertificateData>>("std::vector<plugin::CertificateData>");
}

AsyncCardReader::~AsyncCardReader()
{
    stopRequested = true;
    if (futureData.valid())
        futureData.wait();
    if (futurePKI.valid())
        futurePKI.wait();
}

plugin::CardPlugin* AsyncCardReader::currentPlugin() const
{
    return activePlugin;
}

void AsyncCardReader::requestData()
{
    if (futureData.valid())
        return;

    emit readingStarted();

    futureData = std::async(std::launch::async, [this]() {
        if (!conn) {
            QMetaObject::invokeMethod(
                this,
                [this]() {
                    emit errorOccurred(tr("No card connection available"));
                    emit readingFinished();
                },
                Qt::QueuedConnection);
            return;
        }

        for (auto* candidate : candidates) {
            if (stopRequested)
                return;
            try {
                auto data = candidate->readCard(*conn);
                QMetaObject::invokeMethod(
                    this,
                    [this, data = std::move(data), candidate]() {
                        activePlugin = candidate;
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
            this,
            [this]() {
                emit errorOccurred(tr("No plugin could read this card."));
                emit readingFinished();
            },
            Qt::QueuedConnection);
    });
}

void AsyncCardReader::requestCertificates()
{
    if (!activePlugin || !activePlugin->supportsPKI())
        return;

    if (futurePKI.valid())
        futurePKI.wait();

    futurePKI = std::async(std::launch::async, [this]() {
        if (stopRequested)
            return;
        try {
            auto certs = activePlugin->readCertificates(*conn);
            QMetaObject::invokeMethod(
                this, [this, certs = std::move(certs)]() { emit certificatesReady(certs); }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                this, [this, msg = QString::fromStdString(e.what())]() { emit errorOccurred(msg); },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestPINTriesLeft()
{
    if (!activePlugin || !activePlugin->supportsPKI())
        return;

    if (futurePKI.valid())
        futurePKI.wait();

    futurePKI = std::async(std::launch::async, [this]() {
        if (stopRequested)
            return;
        try {
            int tries = activePlugin->getPINTriesLeft(*conn);
            QMetaObject::invokeMethod(
                this, [this, tries]() { emit pinStatusReady(tries, tries == 0); }, Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                this, [this, msg = QString::fromStdString(e.what())]() { emit errorOccurred(msg); },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestChangePIN(const QString& oldPin, const QString& newPin)
{
    if (!activePlugin || !activePlugin->supportsPKI())
        return;

    if (futurePKI.valid())
        futurePKI.wait();

    auto oldPinStd = oldPin.toStdString();
    auto newPinStd = newPin.toStdString();

    futurePKI = std::async(std::launch::async, [this, oldPinStd, newPinStd]() {
        if (stopRequested)
            return;
        try {
            auto result = activePlugin->changePIN(*conn, oldPinStd, newPinStd);
            QMetaObject::invokeMethod(
                this,
                [this, result]() {
                    emit pinChangeResult(result.success, result.retriesLeft,
                                         result.success ? QString() : tr("PIN change failed."));
                },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                this, [this, msg = QString::fromStdString(e.what())]() { emit pinChangeResult(false, -1, msg); },
                Qt::QueuedConnection);
        }
    });
}

void AsyncCardReader::requestVerifyPIN(const QString& pin)
{
    if (!activePlugin || !activePlugin->supportsPKI())
        return;

    if (futurePKI.valid())
        futurePKI.wait();

    auto pinStd = pin.toStdString();

    futurePKI = std::async(std::launch::async, [this, pinStd]() {
        if (stopRequested)
            return;
        try {
            auto result = activePlugin->verifyPIN(*conn, pinStd);
            QMetaObject::invokeMethod(
                this, [this, result]() { emit pinVerifyResult(result.success, result.retriesLeft); },
                Qt::QueuedConnection);
        } catch (const std::exception& e) {
            QMetaObject::invokeMethod(
                this, [this, msg = QString::fromStdString(e.what())]() { emit errorOccurred(msg); },
                Qt::QueuedConnection);
        }
    });
}
