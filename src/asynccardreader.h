// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#pragma once

#include <plugin/card_plugin.h>

#include <QMap>
#include <QObject>

#include <atomic>
#include <future>
#include <memory>
#include <vector>

namespace smartcard {
class PCSCConnection;
}

class AsyncCardReader : public QObject
{
    Q_OBJECT
public:
    // candidates: middleware plugins sorted by priority (caller retains ownership)
    // conn: ownership transferred to AsyncCardReader
    AsyncCardReader(std::vector<plugin::CardPlugin*> candidates, std::unique_ptr<smartcard::PCSCConnection> conn,
                    QObject* parent = nullptr);
    ~AsyncCardReader();

    void requestData();
    void requestCertificates();
    void requestPINTriesLeft();
    void requestPINTriesLeft(uint8_t pinReference);
    void requestChangePIN(const QString& oldPin, const QString& newPin);
    void requestPINList();
    void requestChangePIN(uint8_t pinReference, const QString& oldPin, const QString& newPin);
    void requestVerifyPIN(const QString& pin);
    void requestDataWithCredentials(const QMap<QString, QString>& credentials);

    // Signal async threads to stop and block until they finish.
    // Safe to call multiple times. Called automatically by the destructor.
    void cancel();

    plugin::CardPlugin* currentPlugin() const;
    bool hasPKI() const;
    void clearPluginCredentials();

signals:
    void cardGroupReady(const QString& cardType, const plugin::CardFieldGroup& group);
    void cardDataReady(const plugin::CardData& data);
    void certificatesReady(const std::vector<plugin::CertificateData>& certs);
    void pinStatusReady(int triesLeft, bool blocked);
    void pinChangeResult(bool success, int triesLeft, const QString& errorMessage);
    void pinVerifyResult(bool success, int triesLeft);
    void pinListReady(const std::vector<plugin::PinStatusEntry>& pins);
    void errorOccurred(const QString& message);
    void readingStarted();
    void readingFinished();

private:
    // Ensure no async operations are using conn before launching a new one.
    // Only ONE thread may access conn at a time (PCSCConnection has no internal synchronization).
    void waitForPendingAsync();

    std::vector<plugin::CardPlugin*> candidates;

    // Plugin pointers are safe to capture in async lambdas because plugin objects
    // are owned by CardPluginRegistry/CardWidgetPluginRegistry and live for the
    // application's lifetime. They are never unloaded or deallocated during runtime.
    plugin::CardPlugin* activePlugin = nullptr;
    plugin::CardPlugin* pkiPlugin = nullptr;
    std::unique_ptr<smartcard::PCSCConnection> conn;
    std::atomic<bool> stopRequested{false};
    bool certsAlreadyQueued = false;
    std::future<void> futureData;
    std::future<void> futurePKI;
};
