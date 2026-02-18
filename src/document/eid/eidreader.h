// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDREADER_H
#define EIDREADER_H

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

    void requestData();

signals:
    void cardTypeRead(eidcard::CardType cardType);
    void fixedPersonalDataRead(eidcard::FixedPersonalData fixedPersonalData);
    void variablePersonalDataRead(eidcard::VariablePersonalData variablePersonalData);
    void documentDataRead(eidcard::DocumentData documentData);
    void photoDataRead(eidcard::PhotoData photoData);
    void cardVerificationResultRead(eidcard::VerificationResult verificationResult);
    void fixedVerificationResultRead(eidcard::VerificationResult verificationResult);
    void variableVerificationResultRead(eidcard::VerificationResult verificationResult);

    void readingStarted();
    void readingFinished();

protected:
    void requestEIdData(std::unique_ptr<eidcard::EIdCard>& eidCard);
    void requestPhoto(std::unique_ptr<eidcard::EIdCard>& eidCard);
    void requestVerification(std::unique_ptr<eidcard::EIdCard>& eidCard, LibreSCRS::VerificationOptions options);

    std::unique_ptr<eidcard::EIdCard> initEIdCard();

private:
    std::string cardReader;
    std::future<void> futureData;

    static std::mutex cardAccessMutex;
    static std::condition_variable cv;
    static bool processing;
};

#endif // EIDREADER_H
