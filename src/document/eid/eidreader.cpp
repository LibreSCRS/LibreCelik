// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef NDEBUG
#include <sstream>
#include <thread>
#endif

#include "utils/libreceliklog.h"
#include <eidcard/eidcard.h>
#include "eidreader.h"

// Everything in one session (begin-end read)
// must complete before another session begins
std::mutex EIdReader::cardAccessMutex;
std::condition_variable EIdReader::cv;
bool EIdReader::processing = false;

EIdReader::EIdReader(const std::string& cardReader, QObject *parent) : cardReader(cardReader), QObject(parent)
{
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
            requestVerification(eidCard, CelikAPI::VerificationOption::CheckCard | CelikAPI::VerificationOption::CheckSignature);
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
    std::unique_ptr<eidcard::EIdCard> eidCard = nullptr;
    try
    {
        eidCard = std::make_unique<eidcard::EIdCard>(cardReader);
    }
    catch (std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not init eID card on reader: " << cardReader << ". Exception: " << re.what();
    }
    return eidCard;
}

CelikAPI::CardVersion EIdReader::readCardVersion(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    if (!eidCard)
        return CelikAPI::CardVersion::Unknown;
    return static_cast<CelikAPI::CardVersion>(static_cast<int>(eidCard->getCardType()));
}

CelikAPI::FixedPersonalData EIdReader::readFixedPersonalData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    CelikAPI::FixedPersonalData fixedPersonalData;
    if (!eidCard)
        return fixedPersonalData;

    try
    {
        auto fpd = eidCard->readFixedPersonalData();

        fixedPersonalData.givenName = QString::fromStdString(fpd.givenName);
        fixedPersonalData.surname = QString::fromStdString(fpd.surname);
        fixedPersonalData.parentGivenName = QString::fromStdString(fpd.parentGivenName);
        fixedPersonalData.nationalityFull = QString::fromStdString(fpd.nationalityFull);
        fixedPersonalData.sex = QString::fromStdString(fpd.sex);
        fixedPersonalData.personalNumber = QString::fromStdString(fpd.personalNumber);
        fixedPersonalData.dateOfBirth = QString::fromStdString(fpd.dateOfBirth);
        fixedPersonalData.statusOfForeigner = QString::fromStdString(fpd.statusOfForeigner);
        QStringList list;
        list << QString::fromStdString(fpd.placeOfBirth)
             << QString::fromStdString(fpd.communityOfBirth)
             << QString::fromStdString(fpd.stateOfBirth);
        fixedPersonalData.placeOfBirth = list.join(", ");
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read fixed personal data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return fixedPersonalData;
}

CelikAPI::VariablePersonalData EIdReader::readVariablePersonalData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    CelikAPI::VariablePersonalData variablePersonalData;
    if (!eidCard)
        return variablePersonalData;

    try
    {
        auto vpd = eidCard->readVariablePersonalData();

        variablePersonalData.addressDate = QString::fromStdString(vpd.addressDate);
        QStringList list;
        list << QString::fromStdString(vpd.place)
             << QString::fromStdString(vpd.community)
             << QString::fromStdString(vpd.street)
             << QString::fromStdString(vpd.houseNumber);
        QString apartmentNumber = QString::fromStdString(vpd.apartmentNumber);
        QString floor = QString::fromStdString(vpd.floor);
        auto address = floor.isEmpty() ? list.join(", ") : list.join(", ") + "/" + floor;
        variablePersonalData.address = apartmentNumber.isEmpty() ? address : address + "/" + apartmentNumber;
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read variable personal data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return variablePersonalData;
}

CelikAPI::DocumentData EIdReader::readDocumentData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    CelikAPI::DocumentData documentData;
    if (!eidCard)
        return documentData;

    try
    {
        auto doc = eidCard->readDocumentData();
        documentData.docRegNo = QString::fromStdString(doc.docRegNo);
        documentData.issuingAuthority = QString::fromStdString(doc.issuingAuthority);
        documentData.issuingDate = QString::fromStdString(doc.issuingDate);
        documentData.expiryDate = QString::fromStdString(doc.expiryDate);
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read document data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return documentData;
}

CelikAPI::PhotoData EIdReader::readPhotoData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    CelikAPI::PhotoData photoData;
    if (!eidCard)
        return photoData;

    try
    {
        photoData = eidCard->readPortrait();
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read photo on reader: " << cardReader << ". Exception: " << re.what();
    }
    return photoData;
}

CelikAPI::VerificationResult EIdReader::verifyData(std::unique_ptr<eidcard::EIdCard>& eidCard, int option)
{
    // Signature verification is deferred - return Unknown for now
    Q_UNUSED(eidCard);
    Q_UNUSED(option);
    return CelikAPI::VerificationResult::Unknown;
}

void EIdReader::requestEIdData(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    auto cardVersion = readCardVersion(eidCard);
    emit cardVersionRead(cardVersion);

    auto fixedPersonalData = readFixedPersonalData(eidCard);
    emit fixedPersonalDataRead(fixedPersonalData);

    auto variablePersonalData = readVariablePersonalData(eidCard);
    emit variablePersonalDataRead(variablePersonalData);

    auto documentData = readDocumentData(eidCard);
    emit documentDataRead(documentData);
}

void EIdReader::requestVerification(std::unique_ptr<eidcard::EIdCard>& eidCard, CelikAPI::VerificationOptions options)
{
    auto checkCard = options.testFlag(CelikAPI::VerificationOption::CheckCard);
    if (checkCard)
    {
        auto cardSignatureCheckResult = verifyData(eidCard, EID_SIG_CARD);
        emit cardSignatureVerificationResultRead(cardSignatureCheckResult);
    }
    auto checkSignature = options.testFlag(CelikAPI::VerificationOption::CheckSignature);
    if (checkSignature)
    {
        auto fixedSignatureCheckResult = verifyData(eidCard, EID_SIG_FIXED);
        emit fixedSignatureVerificationResultRead(fixedSignatureCheckResult);

        auto variableSignatureCheckResult = verifyData(eidCard, EID_SIG_VARIABLE);
        emit variableSignatureVerificationResultRead(variableSignatureCheckResult);
    }
}

void EIdReader::requestPhoto(std::unique_ptr<eidcard::EIdCard>& eidCard)
{
    auto photoData = readPhotoData(eidCard);
    emit photoDataRead(photoData);
}
