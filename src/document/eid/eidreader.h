// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDREADER_H
#define EIDREADER_H

#include <future>
#include <memory>
#include <string>
#include <QObject>
#include "celikapi/celikapiplus.h"

namespace eidcard {
class EIdCard;
}

class EIdReader : public QObject
{
    Q_OBJECT
public:
    EIdReader(const std::string& cardReader, QObject *parent = nullptr);
    ~EIdReader();

    void requestData();

signals:
    void cardVersionRead(CelikAPI::CardVersion cardVersion);
    void fixedPersonalDataRead(CelikAPI::FixedPersonalData fixedPersonalData);
    void variablePersonalDataRead(CelikAPI::VariablePersonalData variablePersonalData);
    void documentDataRead(CelikAPI::DocumentData documentData);
    void photoDataRead(CelikAPI::PhotoData photoData);
    void cardSignatureVerificationResultRead(CelikAPI::VerificationResult verificationResult);
    void fixedSignatureVerificationResultRead(CelikAPI::VerificationResult verificationResult);
    void variableSignatureVerificationResultRead(CelikAPI::VerificationResult verificationResult);

    void readingStarted();
    void readingFinished();

protected:
    void requestEIdData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    void requestPhoto(std::unique_ptr<eidcard::EIdCard>& eidCard);
    void requestVerification(std::unique_ptr<eidcard::EIdCard>& eidCard, CelikAPI::VerificationOptions options);

    std::unique_ptr<eidcard::EIdCard> initEIdCard();
    CelikAPI::CardVersion readCardVersion(std::unique_ptr<eidcard::EIdCard>& eidCard);
    CelikAPI::FixedPersonalData readFixedPersonalData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    CelikAPI::VariablePersonalData readVariablePersonalData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    CelikAPI::DocumentData readDocumentData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    CelikAPI::PhotoData readPhotoData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    CelikAPI::VerificationResult verifyData(std::unique_ptr<eidcard::EIdCard>& eidCard, int option);

private:
    std::string cardReader;
    std::future<void> futureData;

    static std::mutex cardAccessMutex;
    static std::condition_variable cv;
    static bool processing;
};

#endif // EIDREADER_H
