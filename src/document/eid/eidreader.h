// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDREADER_H
#define EIDREADER_H

#include <atomic>
#include <future>
#include <memory>
#include <string>
#include <QObject>
#include <eidcard/eidtypes.h>
#include "libreceliktypes.h"

namespace eidcard {
class EIdCard;
}

class EIdReader : public QObject
{
    Q_OBJECT
public:
    EIdReader(const std::string& cardReader, QObject *parent = nullptr);
    ~EIdReader();

    const std::string& getReaderName() const { return cardReader; }

    void requestData();
    void requestPINTriesLeft();
    void requestChangePIN(const QString& oldPin, const QString& newPin);

signals:
    void cardTypeRead(eidcard::CardType cardType);
    void fixedPersonalDataRead(eidcard::FixedPersonalData fixedPersonalData);
    void variablePersonalDataRead(eidcard::VariablePersonalData variablePersonalData);
    void documentDataRead(eidcard::DocumentData documentData);
    void photoDataRead(eidcard::PhotoData photoData);
    void certificateDataRead(eidcard::CertificateList certificateList);
    void cardVerificationResultRead(eidcard::VerificationResult verificationResult);
    void fixedVerificationResultRead(eidcard::VerificationResult verificationResult);
    void variableVerificationResultRead(eidcard::VerificationResult verificationResult);

    void pinTriesLeftRead(int triesLeft, bool blocked);
    void pinChangeSuccess();
    void pinChangeFailed(int retriesLeft, bool blocked, const QString& errorMessage);

    void readingStarted();
    void readingFinished();

protected:
    void requestEIdData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    void requestPhoto(std::unique_ptr<eidcard::EIdCard>& eidCard);
    void requestVerification(std::unique_ptr<eidcard::EIdCard>& eidCard, LibreSCRS::VerificationOptions options);

    std::unique_ptr<eidcard::EIdCard> initEIdCard();

private:
    std::string cardReader;
    std::atomic<bool> stopRequested{false};
    std::future<void> futureData;
    std::future<void> futurePinData;

    using EidCardUPtr = std::unique_ptr<eidcard::EIdCard>;
    EidCardUPtr eidCard;
};

#endif // EIDREADER_H
