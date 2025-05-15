// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "utils/libreceliklog.h"
#include "celikapisession.h"
#include "celikapiwrapper.h"

CelikAPISession::CelikAPISession(std::string scReaderName, std::shared_ptr<CelikAPIWrapper> celikApiWrapper)
    : cardReader(scReaderName), celikApiWrapper(celikApiWrapper)
{
    auto ret = celikApiWrapper->beginRead(scReaderName.c_str(), &cardVersion);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "Cannot begin read on reader " << cardReader;
        throw std::runtime_error("Cannot begin read");
    }
    qCDebug(librecSCRSCard) << "Celik API session opened on reader " << cardReader;
}

CelikAPISession::~CelikAPISession() noexcept
{
    auto ret = celikApiWrapper->endRead();
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "Cannot end read on reader " << cardReader;
    }

    qCDebug(librecSCRSCard) << "Closing Celik API session on reader " << cardReader;
}

void CelikAPISession::readDocumentData(PEID_DOCUMENT_DATA pData) const
{
    auto ret = celikApiWrapper->readDocumentData(pData);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "ReadDocumentData error: " << RVToString(ret) << " on reader " << cardReader;
        throw std::runtime_error("ReadDocumentData error on reader " + cardReader);
    }
    qCDebug(librecSCRSCard) << "ReadDocumentData on reader " << cardReader;
}

void CelikAPISession::readFixedPersonalData(PEID_FIXED_PERSONAL_DATA pData) const
{
    auto ret = celikApiWrapper->readFixedPersonalData(pData);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "ReadFixedPersonalData error: " << RVToString(ret) << " on reader " << cardReader;
        throw std::runtime_error("ReadFixedPersonalData error on reader " + cardReader);
    }
    qCDebug(librecSCRSCard) << "ReadFixedPersonalData on reader " << cardReader;
}

void CelikAPISession::readVariablePersonalData(PEID_VARIABLE_PERSONAL_DATA pData) const
{
    auto ret = celikApiWrapper->readVariablePersonalData(pData);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "ReadVariablePersonalData error: " << RVToString(ret) << " on reader " << cardReader;
        throw std::runtime_error("ReadVariablePersonalData error on reader " + cardReader);
    }
    qCDebug(librecSCRSCard) << "ReadVariablePersonalData on reader " << cardReader;
}

void CelikAPISession::readPortrait(PEID_PORTRAIT pData) const
{
    auto ret = celikApiWrapper->readPortrait(pData);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "ReadPortrait error: " << RVToString(ret) << " on reader " << cardReader;
        throw std::runtime_error("ReadPortrait error on reader " + cardReader);
    }
    qCDebug(librecSCRSCard) << "ReadPortrait on reader " << cardReader;
}

// void CelikAPISession::ReadCertificate(PEID_CERTIFICATE pData, int certificateType) const
// {
//     auto ret = celikApiWrapper->ReadCertificate(pData, certificateType);
//     if (ret != CelikAPI::CelikAPIReturnValue::OK)
//     {
//         qCCritical(librecelikAPI) << "ReadCertificate error: " << RVToString(ret);
//         throw std::runtime_error("ReadCertificate error");
//     }
//     qCDebug(librecelikAPI) << "ReadCertificate";
// }

// void CelikAPISession::ChangePassword(LPCSTR szOldPassword, LPCSTR szNewPassword, int* pnTriesLeft) const
// {
//     auto ret = celikApiWrapper->ChangePassword(szOldPassword, szNewPassword, pnTriesLeft);
//     if (ret != CelikAPI::CelikAPIReturnValue::OK)
//     {
//         qCCritical(librecelikAPI) << "ChangePassword error: " << RVToString(ret);
//         throw std::runtime_error("ChangePassword error");
//     }
//     qCDebug(librecelikAPI) << "ChangePassword";
// }

void CelikAPISession::verifySignature(uint8_t nSignatureID) const
{
    auto ret = celikApiWrapper->verifySignature(nSignatureID);
    if (ret != CelikAPI::CelikAPIReturnValue::OK)
    {
        qCCritical(librecSCRSCard) << "VerifySignature error: " << RVToString(ret) << " on reader " << cardReader;
        throw std::runtime_error("VerifySignature error on reader " + cardReader);
    }
    qCDebug(librecSCRSCard) << "VerifySignature on reader " << cardReader;
}
