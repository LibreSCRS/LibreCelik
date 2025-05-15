// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "celikapiwrapper.h"

CelikAPIWrapper::CelikAPIWrapper() {}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::startup(int nApiVersion) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidStartup(nApiVersion));
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::cleanup() const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidCleanup());
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::beginRead(LPCSTR szReader, int* pnCardType) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidBeginRead(szReader, pnCardType));
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::endRead() const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidEndRead());
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::readDocumentData(PEID_DOCUMENT_DATA pData) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidReadDocumentData(pData));
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::readFixedPersonalData(PEID_FIXED_PERSONAL_DATA pData) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidReadFixedPersonalData(pData));
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::readVariablePersonalData(PEID_VARIABLE_PERSONAL_DATA pData) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidReadVariablePersonalData(pData));
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::readPortrait(PEID_PORTRAIT pData) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidReadPortrait(pData));
}

// CelikAPI::CelikAPIReturnValue CelikAPIWrapper::ReadCertificate(PEID_CERTIFICATE pData, int certificateType) const
// {
//     return static_cast<CelikAPI::CelikAPIReturnValue> (EidReadCertificate(pData, certificateType));
// }

// CelikAPI::CelikAPIReturnValue CelikAPIWrapper::ChangePassword(LPCSTR szOldPassword, LPCSTR szNewPassword, int* pnTriesLeft) const
// {
//     return static_cast<CelikAPI::CelikAPIReturnValue> (EidChangePassword(szOldPassword, szNewPassword, pnTriesLeft));
// }

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::verifySignature(uint8_t nSignatureID) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidVerifySignature(nSignatureID));
}

CelikAPI::CelikAPIReturnValue CelikAPIWrapper::setOption(int nOptionID, uintptr_t nOptionValue) const
{
    std::scoped_lock lock(cardAccessMutex);
    return static_cast<CelikAPI::CelikAPIReturnValue> (EidSetOption(nOptionID, nOptionValue));
}
