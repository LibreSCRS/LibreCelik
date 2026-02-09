// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDCARD_EIDCARD_H
#define EIDCARD_EIDCARD_H

#include <memory>
#include <string>
#include "eidtypes.h"

namespace smartcard {
class PCSCConnection;
}

namespace eidcard {

class CardReaderBase;

class EIdCard {
public:
    explicit EIdCard(const std::string& readerName);
    ~EIdCard();

    EIdCard(const EIdCard&) = delete;
    EIdCard& operator=(const EIdCard&) = delete;

    CardType getCardType() const;
    DocumentData readDocumentData();
    FixedPersonalData readFixedPersonalData();
    VariablePersonalData readVariablePersonalData();
    PhotoData readPortrait();

private:
    std::unique_ptr<smartcard::PCSCConnection> connection_;
    std::unique_ptr<CardReaderBase> cardReader_;
    CardType cardType_ = CardType::Unknown;
};

} // namespace eidcard

#endif // EIDCARD_EIDCARD_H
