// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef HEALTHREADER_H
#define HEALTHREADER_H

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <QObject>
#include <healthcard/healthtypes.h>
#include <eidcard/eidtypes.h>

namespace healthcard {
class HealthCard;
}

class HealthReader : public QObject
{
    Q_OBJECT
public:
    HealthReader(const std::string& cardReader, QObject* parent = nullptr);
    ~HealthReader();

    void requestData();
    void requestCertificates();
    void requestPINTriesLeft();
    void requestChangePIN(const QString& oldPin, const QString& newPin);

signals:
    void healthDataRead(healthcard::HealthDocumentData healthData);
    void certificateDataRead(eidcard::CertificateList certificateList);
    void pinTriesLeftRead(int triesLeft, bool blocked);
    void pinChangeSuccess();
    void pinChangeFailed(int retriesLeft, bool blocked, const QString& errorMessage);

    void readingStarted();
    void readingFinished();

private:
    std::unique_ptr<healthcard::HealthCard> initHealthCard();
    healthcard::HealthDocumentData readHealthData(std::unique_ptr<healthcard::HealthCard>& card);

    std::string cardReader;
    std::atomic<bool> stopRequested{false};
    std::future<void> futureData;
    std::future<void> futureCerts;
    std::future<void> futurePinData;
};

#endif // HEALTHREADER_H
