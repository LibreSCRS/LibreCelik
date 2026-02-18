// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef NDEBUG
#include <sstream>
#include <thread>
#endif

#include "utils/libreceliklog.h"
#include <eidcard/eidcard.h>
#include "eidreader.h"
#include "config.h"

// Everything in one session (begin-end read)
// must complete before another session begins
std::mutex EIdReader::cardAccessMutex;
std::condition_variable EIdReader::cv;
bool EIdReader::processing = false;

EIdReader::EIdReader(const std::string& cardReader, QObject *parent) : cardReader(cardReader), QObject(parent)
{
    qRegisterMetaType<eidcard::CardType>();
    qRegisterMetaType<eidcard::DocumentData>();
    qRegisterMetaType<eidcard::FixedPersonalData>();
    qRegisterMetaType<eidcard::VariablePersonalData>();
    qRegisterMetaType<eidcard::PhotoData>();
    qRegisterMetaType<eidcard::VerificationResult>();
}

EIdReader::~EIdReader()
{
}

void EIdReader::requestData()
{
    futureData = std::async(std::launch::async, [this]() {
        std::unique_lock<std::mutex> lock (cardAccessMutex);
        cv.wait(lock, [this](){return processing == false;});
        processing = true;

        emit readingStarted();

#ifndef NDEBUG
        std::stringstream ss;
        ss << std::this_thread::get_id();

        qDebug(libreCelikAPI) << "EId requestData: " << cardReader << ". Thread: " <<  ss.str();
#endif

        auto eidCard = initEIdCard();
        if (eidCard != nullptr)
        {
            requestEIdData(eidCard);
            requestPhoto(eidCard);
            requestVerification(eidCard, LibreSCRS::VerificationOption::CheckCard | LibreSCRS::VerificationOption::CheckSignature);
        }
        else
        {
            qDebug(libreCelikAPI) << "Can not initialize eID card session on: " << cardReader;
        }

        emit readingFinished();

        processing = false;
        lock.unlock();
        cv.notify_one();
    });
}

std::unique_ptr<eidcard::EIdCard> EIdReader::initEIdCard()
{
    // Retry with delay — the card may not be ready immediately after a swap
    for (int attempt = 0; attempt < 3; attempt++)
    {
        try
        {
            return std::make_unique<eidcard::EIdCard>(cardReader);
        }
        catch (std::runtime_error& re)
        {
            if (attempt < 2) {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
            } else {
                qCWarning(libreCelikAPI) << "Can not init eID card on reader: " << cardReader << ". Exception: " << re.what();
            }
        }
    }
    return nullptr;
}

void EIdReader::requestEIdData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    if (!eidCard)
        return;

    emit cardTypeRead(eidCard->getCardType());

    try
    {
        emit fixedPersonalDataRead(eidCard->readFixedPersonalData());
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read fixed personal data on reader: " << cardReader << ". Exception: " << re.what();
    }

    try
    {
        emit variablePersonalDataRead(eidCard->readVariablePersonalData());
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read variable personal data on reader: " << cardReader << ". Exception: " << re.what();
    }

    try
    {
        emit documentDataRead(eidCard->readDocumentData());
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read document data on reader: " << cardReader << ". Exception: " << re.what();
    }
}

void EIdReader::requestVerification(std::unique_ptr<eidcard::EIdCard>& eidCard, LibreSCRS::VerificationOptions options)
{
    if (!eidCard)
        return;

    eidCard->setCertificateFolderPath(LIBRECELIK_CERTIFICATES_DIR);
    qDebug(libreCelikAPI) << "Verification certificates path:" << LIBRECELIK_CERTIFICATES_DIR;

    if (options.testFlag(LibreSCRS::VerificationOption::CheckCard))
    {
        try {
            emit cardVerificationResultRead(eidCard->verifyCard());
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Card verification failed:" << e.what();
            emit cardVerificationResultRead(eidcard::VerificationResult::Unknown);
        }
    }

    if (options.testFlag(LibreSCRS::VerificationOption::CheckSignature))
    {
        try {
            emit fixedVerificationResultRead(eidCard->verifyFixedData());
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Fixed data verification failed:" << e.what();
            emit fixedVerificationResultRead(eidcard::VerificationResult::Unknown);
        }

        try {
            emit variableVerificationResultRead(eidCard->verifyVariableData());
        } catch (const std::exception& e) {
            qCWarning(libreCelikAPI) << "Variable data verification failed:" << e.what();
            emit variableVerificationResultRead(eidcard::VerificationResult::Unknown);
        }
    }
}

void EIdReader::requestPhoto(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    if (!eidCard)
        return;

    try
    {
        emit photoDataRead(eidCard->readPortrait());
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read photo on reader: " << cardReader << ". Exception: " << re.what();
    }
}
