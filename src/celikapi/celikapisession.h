// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKAPISESSION_H
#define CELIKAPISESSION_H

#include <string>
#include "celikapi.h"

class CelikAPIWrapper;

class CelikAPISession
{
public:
    CelikAPISession(std::string scReaderName, std::shared_ptr<CelikAPIWrapper> celikAPIWrapper);
    ~CelikAPISession() noexcept;

    void readDocumentData(PEID_DOCUMENT_DATA pData) const;
    void readFixedPersonalData(PEID_FIXED_PERSONAL_DATA pData) const;
    void readVariablePersonalData(PEID_VARIABLE_PERSONAL_DATA pData) const;
    void readPortrait(PEID_PORTRAIT pData) const;
    //void ReadCertificate(PEID_CERTIFICATE pData, int certificateType) const;
    //void ChangePassword(LPCSTR szOldPassword, LPCSTR szNewPassword, int* pnTriesLeft = nullptr) const;
    void verifySignature(uint8_t nSignatureID) const;

    inline int getCardVersion() const { return cardVersion; }

private:
    std::shared_ptr<CelikAPIWrapper> celikApiWrapper = nullptr;
    std::string cardReader;
    int cardVersion = 0;
};

#endif // CELIKAPISESSION_H
