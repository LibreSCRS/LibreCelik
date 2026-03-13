// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef PKSREADER_H
#define PKSREADER_H

#include <future>
#include <string>
#include <QObject>
#include <eidcard/eidtypes.h>

class PKSReader : public QObject
{
    Q_OBJECT
public:
    PKSReader(const std::string& cardReader, QObject* parent = nullptr);
    ~PKSReader();

    void requestCertificates();
    void requestPINTriesLeft();
    void requestChangePIN(const QString& oldPin, const QString& newPin);

signals:
    void certificateDataRead(eidcard::CertificateList certificateList);
    void pinTriesLeftRead(int triesLeft, bool blocked);
    void pinChangeSuccess();
    void pinChangeFailed(int retriesLeft, bool blocked, const QString& errorMessage);

    void readingStarted();
    void readingFinished();

private:
    std::string cardReader;
    std::future<void> futureCerts;
    std::future<void> futurePinData;
};

#endif // PKSREADER_H
