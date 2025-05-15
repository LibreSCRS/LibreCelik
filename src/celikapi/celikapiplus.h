// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKAPIPLUS_H
#define CELIKAPIPLUS_H

#include <QString>
#include "celikapi.h"

namespace CelikAPI {
enum class CelikAPIReturnValue : int
{
    OK = EID_OK,
    GeneralError = EID_E_GENERAL_ERROR,
    InvalidParameter = EID_E_INVALID_PARAMETER,
    VersionNotSupported = EID_E_VERSION_NOT_SUPPORTED,
    NotInitialized = EID_E_NOT_INITIALIZED,
    UnableToExecute = EID_E_UNABLE_TO_EXECUTE,
    ReaderError = EID_E_READER_ERROR,
    CardMissing = EID_E_CARD_MISSING,
    CardUnknown = EID_E_CARD_UNKNOWN,
    CardMismatch = EID_E_CARD_MISMATCH,
    UnableToOpenSession = EID_E_UNABLE_TO_OPEN_SESSION,
    DataMissing = EID_E_DATA_MISSING,
    SecFormatCheckError = EID_E_CARD_SECFORMAT_CHECK_ERROR,
    SecFormatCheckCertError = EID_E_SECFORMAT_CHECK_CERT_ERROR,
    InvalidPassword = EID_E_INVALID_PASSWORD,
    PinBlocked = EID_E_PIN_BLOCKED,
};

constexpr const char* RVToString(CelikAPIReturnValue rv) noexcept
{
    switch (rv)
    {
    case CelikAPIReturnValue::OK: return "EID_OK";
    case CelikAPIReturnValue::GeneralError: return "EID_E_GENERAL_ERROR";
    case CelikAPIReturnValue::InvalidParameter: return "EID_E_INVALID_PARAMETER";
    case CelikAPIReturnValue::VersionNotSupported: return "EID_E_VERSION_NOT_SUPPORTED";
    case CelikAPIReturnValue::NotInitialized: return "EID_E_NOT_INITIALIZED";
    case CelikAPIReturnValue::UnableToExecute: return "EID_E_UNABLE_TO_EXECUTE";
    case CelikAPIReturnValue::ReaderError: return "EID_E_READER_ERROR";
    case CelikAPIReturnValue::CardMissing: return "EID_E_CARD_MISSING";
    case CelikAPIReturnValue::CardUnknown: return "EID_E_CARD_UNKNOWN";
    case CelikAPIReturnValue::CardMismatch: return "EID_E_CARD_MISMATCH";
    case CelikAPIReturnValue::UnableToOpenSession: return "EID_E_UNABLE_TO_OPEN_SESSION";
    case CelikAPIReturnValue::DataMissing: return "EID_E_DATA_MISSING";
    case CelikAPIReturnValue::SecFormatCheckError: return "EID_E_CARD_SECFORMAT_CHECK_ERROR";
    case CelikAPIReturnValue::SecFormatCheckCertError: return "EID_E_SECFORMAT_CHECK_CERT_ERROR";
    case CelikAPIReturnValue::InvalidPassword: return "EID_E_INVALID_PASSWORD";
    case CelikAPIReturnValue::PinBlocked: return "EID_E_PIN_BLOCKED";
    default: return "Unknown error";
    }
}

enum class CardVersion : int
{
    Unknown = 0,
    Card2008 = 1, //EID_CARD_ID2008,
    Card2014 = 2, //EID_CARD_ID2014,
    CardIF2020 = 3, //EID_CARD_IF2020
};

struct DocumentData
{
    QString docRegNo;
    QString issuingAuthority;
    QString issuingDate;
    QString expiryDate;
};

struct VariablePersonalData
{
    QString address;
    QString addressDate;
};

struct FixedPersonalData
{
    QString givenName;
    QString surname;
    QString parentGivenName;
    QString nationalityFull;
    QString sex;
    QString personalNumber;
    QString dateOfBirth;
    QString statusOfForeigner;
    QString placeOfBirth;
};

using PhotoData = std::vector<uint8_t>;

enum VerificationOption : uint8_t
{
    NoCheck = 1 << 1,
    CheckCard = 1 << 2,
    CheckSignature = 1 << 3
};
Q_DECLARE_FLAGS(VerificationOptions, VerificationOption);
Q_DECLARE_OPERATORS_FOR_FLAGS(VerificationOptions);

enum class VerificationResult
{
    Unknown,
    Good,
    Bad
};

}

#endif // CELIKAPIPLUS_H
