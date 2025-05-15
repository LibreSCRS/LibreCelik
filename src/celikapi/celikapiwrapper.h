// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKAPIWRAPPER_H
#define CELIKAPIWRAPPER_H

#include "celikapiplus.h"

class CelikAPIWrapper
{
public:
    CelikAPIWrapper();

    CelikAPI::CelikAPIReturnValue startup(int nApiVersion) const;
    CelikAPI::CelikAPIReturnValue cleanup() const;

    CelikAPI::CelikAPIReturnValue beginRead(LPCSTR szReader, int* pnCardType = 0) const;
    CelikAPI::CelikAPIReturnValue endRead() const;

    CelikAPI::CelikAPIReturnValue readDocumentData(PEID_DOCUMENT_DATA pData) const;
    CelikAPI::CelikAPIReturnValue readFixedPersonalData(PEID_FIXED_PERSONAL_DATA pData) const;
    CelikAPI::CelikAPIReturnValue readVariablePersonalData(PEID_VARIABLE_PERSONAL_DATA pData) const;
    CelikAPI::CelikAPIReturnValue readPortrait(PEID_PORTRAIT pData) const;
    // CelikAPI::CelikAPIReturnValue ReadCertificate(PEID_CERTIFICATE pData, int certificateType) const;
    // CelikAPI::CelikAPIReturnValue ChangePassword(LPCSTR szOldPassword, LPCSTR szNewPassword, int* pnTriesLeft) const;
    CelikAPI::CelikAPIReturnValue verifySignature(uint8_t nSignatureID) const;

    CelikAPI::CelikAPIReturnValue setOption(int nOptionID, uintptr_t nOptionValue) const;

private:
    mutable std::mutex cardAccessMutex;
};

#endif // CELIKAPIWRAPPER_H
