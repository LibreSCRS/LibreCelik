// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef ASYNCCARDREADER_H
#define ASYNCCARDREADER_H

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
    void requestChangePIN(const QString& oldPin, const QString& newPin);
    void requestVerifyPIN(const QString& pin);
    void requestDataWithCredentials(const QMap<QString, QString>& credentials);

    // Signal async threads to stop and block until they finish.
    // Safe to call multiple times. Called automatically by the destructor.
    void cancel();

    plugin::CardPlugin* currentPlugin() const;
    bool hasPKI() const;

signals:
    void cardGroupReady(const QString& cardType, const plugin::CardFieldGroup& group);
    void cardDataReady(const plugin::CardData& data);
    void certificatesReady(const std::vector<plugin::CertificateData>& certs);
    void pinStatusReady(int triesLeft, bool blocked);
    void pinChangeResult(bool success, int triesLeft, const QString& errorMessage);
    void pinVerifyResult(bool success, int triesLeft);
    void errorOccurred(const QString& message);
    void readingStarted();
    void readingFinished();

private:
    std::vector<plugin::CardPlugin*> candidates;
    plugin::CardPlugin* activePlugin = nullptr;
    plugin::CardPlugin* pkiPlugin = nullptr;
    std::unique_ptr<smartcard::PCSCConnection> conn;
    std::atomic<bool> stopRequested{false};
    bool certsAlreadyQueued = false;
    std::future<void> futureData;
    std::future<void> futurePKI;
};

#endif // ASYNCCARDREADER_H
