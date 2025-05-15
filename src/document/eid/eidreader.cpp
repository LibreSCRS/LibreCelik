// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef NDEBUG
#include <sstream>
#include <thread>
#endif

#include "utils/libreceliklog.h"
#include "celikapi/celikapisession.h"
#include "celikapi/celikapisessionfactory.h"
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

        auto celikAPISession = initCelikAPISession();
        if (celikAPISession != nullptr)
        {
            requestEIdData(celikAPISession);
            requestPhoto(celikAPISession);
            requestVerification(celikAPISession, CelikAPI::VerificationOption::CheckCard | CelikAPI::VerificationOption::CheckSignature);
        }
        else
        {
            qDebug(libreCelikAPI) << "Can not initialize celik api session on: " << cardReader;
        }

        emit readingFinished();

        processing = false;
        lock.unlock();
        cv.notify_one();
    });
}

std::shared_ptr<CelikAPISession> EIdReader::initCelikAPISession()
{
    std::shared_ptr<CelikAPISession> celikAPISession = nullptr;
    try
    {
        celikAPISession = CelikAPISessionFactory::instance().createCelikAPISession(cardReader);
    }
    catch (std::runtime_error&re)
    {
        qCWarning(libreCelikAPI) << "Can not init celik api on reader: " << cardReader << ". Exception: " << re.what();
    }
    return celikAPISession;
}

CelikAPI::CardVersion EIdReader::readCardVersion(std::shared_ptr<CelikAPISession> celikAPISession)
{
    if (!celikAPISession)
        return CelikAPI::CardVersion::Unknown;
    return static_cast<CelikAPI::CardVersion>(celikAPISession->getCardVersion());
}

CelikAPI::FixedPersonalData EIdReader::readFixedPersonalData(std::shared_ptr<CelikAPISession> celikAPISession)
{
    CelikAPI::FixedPersonalData fixedPersonalData;
    if (!celikAPISession)
        return fixedPersonalData;

    try
    {
        EID_FIXED_PERSONAL_DATA eidFixedPersonalData;
        celikAPISession->readFixedPersonalData(&eidFixedPersonalData);

        fixedPersonalData.givenName = QString::fromUtf8(eidFixedPersonalData.givenName, eidFixedPersonalData.givenNameSize);
        fixedPersonalData.surname = QString::fromUtf8(eidFixedPersonalData.surname, eidFixedPersonalData.surnameSize);
        fixedPersonalData.parentGivenName = QString::fromUtf8(eidFixedPersonalData.parentGivenName, eidFixedPersonalData.parentGivenNameSize);
        fixedPersonalData.nationalityFull = QString::fromUtf8(eidFixedPersonalData.nationalityFull, eidFixedPersonalData.nationalityFullSize);
        fixedPersonalData.sex = QString::fromUtf8(eidFixedPersonalData.sex, eidFixedPersonalData.sexSize);
        fixedPersonalData.personalNumber = QString::fromUtf8(eidFixedPersonalData.personalNumber, eidFixedPersonalData.personalNumberSize);
        fixedPersonalData.dateOfBirth = QString::fromUtf8(eidFixedPersonalData.dateOfBirth, eidFixedPersonalData.dateOfBirthSize);
        fixedPersonalData.statusOfForeigner = QString::fromUtf8(eidFixedPersonalData.statusOfForeigner, eidFixedPersonalData.statusOfForeignerSize);
        QStringList list;
        list << QString::fromUtf8(eidFixedPersonalData.placeOfBirth, eidFixedPersonalData.placeOfBirthSize)
             << QString::fromUtf8(eidFixedPersonalData.communityOfBirth, eidFixedPersonalData.communityOfBirthSize)
             << QString::fromUtf8(eidFixedPersonalData.stateOfBirth, eidFixedPersonalData.stateOfBirthSize);
        fixedPersonalData.placeOfBirth = list.join(", ");
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read fixed personal data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return fixedPersonalData;
}

CelikAPI::VariablePersonalData EIdReader::readVariablePersonalData(std::shared_ptr<CelikAPISession> celikAPISession)
{
    CelikAPI::VariablePersonalData variablePersonalData;
    if (!celikAPISession)
        return variablePersonalData;

    try
    {
        EID_VARIABLE_PERSONAL_DATA eidVariablePersonalData;
        celikAPISession->readVariablePersonalData(&eidVariablePersonalData);

        variablePersonalData.addressDate = QString::fromUtf8(eidVariablePersonalData.addressDate, eidVariablePersonalData.addressDateSize);
        QStringList list;
        list << QString::fromUtf8(eidVariablePersonalData.place, eidVariablePersonalData.placeSize)
             << QString::fromUtf8(eidVariablePersonalData.community, eidVariablePersonalData.communitySize)
             << QString::fromUtf8(eidVariablePersonalData.street, eidVariablePersonalData.streetSize)
             << QString::fromUtf8(eidVariablePersonalData.houseNumber, eidVariablePersonalData.houseNumberSize);
        QString apartmentNumber = QString::fromUtf8(eidVariablePersonalData.apartmentNumber, eidVariablePersonalData.apartmentNumberSize);
        QString floor = QString::fromUtf8(eidVariablePersonalData.floor, eidVariablePersonalData.floorSize);
        auto address = floor.isEmpty() ? list.join(", ") : list.join(", ") + "/" + floor;
        variablePersonalData.address = apartmentNumber.isEmpty() ? address : address + "/" + apartmentNumber;
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read variable personal data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return variablePersonalData;
}

CelikAPI::DocumentData EIdReader::readDocumentData(std::shared_ptr<CelikAPISession> celikAPISession)
{
    CelikAPI::DocumentData documentData;
    if (!celikAPISession)
        return documentData;

    try
    {
        EID_DOCUMENT_DATA data;
        celikAPISession->readDocumentData(&data);
        documentData.docRegNo = QString::fromUtf8(data.docRegNo, data.docRegNoSize);
        documentData.issuingAuthority = QString::fromUtf8(data.issuingAuthority, data.issuingAuthoritySize);
        documentData.issuingDate = QString::fromUtf8(data.issuingDate, data.issuingDateSize);
        documentData.expiryDate = QString::fromUtf8(data.expiryDate, data.expiryDateSize);
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read document data on reader: " << cardReader << ". Exception: " << re.what();
    }
    return documentData;
}

CelikAPI::PhotoData EIdReader::readPhotoData(std::shared_ptr<CelikAPISession> celikAPISession)
{
    CelikAPI::PhotoData photoData;
    if (!celikAPISession)
        return photoData;

    try
    {
        EID_PORTRAIT eidPortrait;
        celikAPISession->readPortrait(&eidPortrait);

        photoData.resize(eidPortrait.portraitSize);
        std::copy_n(eidPortrait.portrait, eidPortrait.portraitSize, std::begin(photoData));
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not read photo on reader: " << cardReader << ". Exception: " << re.what();
    }
    return photoData;
}

CelikAPI::VerificationResult EIdReader::verifyData(std::shared_ptr<CelikAPISession> celikAPISession, int option)
{
    CelikAPI::VerificationResult vr = CelikAPI::VerificationResult::Unknown;
    if (!celikAPISession)
        return vr;

    try
    {
        celikAPISession->verifySignature(option);
        vr = CelikAPI::VerificationResult::Good;
    }
    catch(std::runtime_error& re)
    {
        qCWarning(libreCelikAPI) << "Can not verify data on reader: " << cardReader << ". Exception: " << re.what();
        vr = CelikAPI::VerificationResult::Bad;
    }
    return vr;
}

void EIdReader::requestEIdData(std::shared_ptr<CelikAPISession> celikAPISession)
{
    auto cardVersion = readCardVersion(celikAPISession);
    emit cardVersionRead(cardVersion);

    auto fixedPersonalData = readFixedPersonalData(celikAPISession);
    emit fixedPersonalDataRead(fixedPersonalData);

    auto variablePersonalData = readVariablePersonalData(celikAPISession);
    emit variablePersonalDataRead(variablePersonalData);

    auto documentData = readDocumentData(celikAPISession);
    emit documentDataRead(documentData);
}

void EIdReader::requestVerification(std::shared_ptr<CelikAPISession> celikAPISession, CelikAPI::VerificationOptions options)
{
    auto checkCard = options.testFlag(CelikAPI::VerificationOption::CheckCard);
    if (checkCard)
    {
        auto cardSignatureCheckResult = verifyData(celikAPISession, EID_SIG_CARD);
        emit cardSignatureVerificationResultRead(cardSignatureCheckResult);
    }
    auto checkSignature = options.testFlag(CelikAPI::VerificationOption::CheckSignature);
    if (checkSignature)
    {
        auto fixedSignatureCheckResult = verifyData(celikAPISession, EID_SIG_FIXED);
        emit fixedSignatureVerificationResultRead(fixedSignatureCheckResult);

        auto variableSignatureCheckResult = verifyData(celikAPISession, EID_SIG_VARIABLE);
        emit variableSignatureVerificationResultRead(variableSignatureCheckResult);
    }
}

void EIdReader::requestPhoto(std::shared_ptr<CelikAPISession> celikAPISession)
{
    auto photoData = readPhotoData(celikAPISession);
    emit photoDataRead(photoData);
}
