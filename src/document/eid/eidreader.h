// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDREADER_H
#define EIDREADER_H

#include <future>
#include <memory>
#include <string>
#include <QObject>
#include "celikapi/celikapiplus.h"

class CelikAPISession;

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
    void requestEIdData(std::shared_ptr<CelikAPISession> celikAPISession);
    void requestPhoto(std::shared_ptr<CelikAPISession> celikAPISession);
    void requestVerification(std::shared_ptr<CelikAPISession> celikAPISession, CelikAPI::VerificationOptions options);

    std::shared_ptr<CelikAPISession> initCelikAPISession();
    CelikAPI::CardVersion readCardVersion(std::shared_ptr<CelikAPISession>);
    CelikAPI::FixedPersonalData readFixedPersonalData(std::shared_ptr<CelikAPISession> celikAPISession);
    CelikAPI::VariablePersonalData readVariablePersonalData(std::shared_ptr<CelikAPISession> celikAPISession);
    CelikAPI::DocumentData readDocumentData(std::shared_ptr<CelikAPISession> celikAPISession);
    CelikAPI::PhotoData readPhotoData(std::shared_ptr<CelikAPISession> celikAPISession);
    CelikAPI::VerificationResult verifyData(std::shared_ptr<CelikAPISession> celikAPISession, int option);

private:
    std::string cardReader;
    std::future<void> futureData;

    static std::mutex cardAccessMutex;
    static std::condition_variable cv;
    static bool processing;
};

#endif // EIDREADER_H
