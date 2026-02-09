// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef CELIKAPIPLUS_H
#define CELIKAPIPLUS_H

#include <QString>

// Signature verification IDs (previously from celikapi.h)
constexpr int EID_SIG_CARD = 1;
constexpr int EID_SIG_FIXED = 2;
constexpr int EID_SIG_VARIABLE = 3;

namespace CelikAPI {

enum class CardVersion : int
{
    Unknown = 0,
    Card2008 = 1,
    Card2014 = 2,
    CardIF2020 = 3,
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
