// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "eidcard/eidcard.h"
#include "card_protocol.h"
#include "card_reader_base.h"
#include "card_reader_gemalto.h"
#include "card_reader_apollo.h"
#include "smartcard/pcsc_connection.h"
#include "smartcard/tlv.h"

namespace eidcard {

EIdCard::EIdCard(const std::string& readerName)
{
    connection_ = std::make_unique<smartcard::PCSCConnection>(readerName);

    // Detect card type from ATR
    auto atr = connection_->getATR();

    if (protocol::isGemaltoATR(atr)) {
        // Gemalto card: try AID selection to determine citizen/foreigner
        cardType_ = CardReaderGemalto::selectApplication(*connection_);
        cardReader_ = std::make_unique<CardReaderGemalto>();
    } else if (protocol::isApolloATR(atr)) {
        cardType_ = CardType::Apollo2008;
        cardReader_ = std::make_unique<CardReaderApollo>();
    } else {
        // Unknown ATR - try Gemalto AID selection as fallback
        cardType_ = CardReaderGemalto::selectApplication(*connection_);
        if (cardType_ != CardType::Unknown) {
            cardReader_ = std::make_unique<CardReaderGemalto>();
        } else {
            throw std::runtime_error("Unknown card type, ATR not recognized");
        }
    }
}

EIdCard::~EIdCard() = default;

CardType EIdCard::getCardType() const
{
    return cardType_;
}

DocumentData EIdCard::readDocumentData()
{
    auto raw = cardReader_->readFile(*connection_,
                                     protocol::FILE_DOCUMENT_DATA_H,
                                     protocol::FILE_DOCUMENT_DATA_L);
    auto fields = smartcard::parseTLV(raw.data(), raw.size());

    DocumentData doc;
    doc.docRegNo            = smartcard::findString(fields, protocol::TAG_DOC_REG_NO);
    doc.documentType        = smartcard::findString(fields, protocol::TAG_DOCUMENT_TYPE);
    doc.documentSerialNumber = smartcard::findString(fields, protocol::TAG_DOCUMENT_SERIAL_NO);
    doc.issuingDate         = smartcard::findString(fields, protocol::TAG_ISSUING_DATE);
    doc.expiryDate          = smartcard::findString(fields, protocol::TAG_EXPIRY_DATE);
    doc.issuingAuthority    = smartcard::findString(fields, protocol::TAG_ISSUING_AUTHORITY);
    doc.chipSerialNumber    = smartcard::findString(fields, protocol::TAG_CHIP_SERIAL_NUMBER);
    return doc;
}

FixedPersonalData EIdCard::readFixedPersonalData()
{
    auto raw = cardReader_->readFile(*connection_,
                                     protocol::FILE_PERSONAL_DATA_H,
                                     protocol::FILE_PERSONAL_DATA_L);
    auto fields = smartcard::parseTLV(raw.data(), raw.size());

    FixedPersonalData fpd;
    fpd.personalNumber   = smartcard::findString(fields, protocol::TAG_PERSONAL_NUMBER);
    fpd.surname          = smartcard::findString(fields, protocol::TAG_SURNAME);
    fpd.givenName        = smartcard::findString(fields, protocol::TAG_GIVEN_NAME);
    fpd.parentGivenName  = smartcard::findString(fields, protocol::TAG_PARENT_GIVEN_NAME);
    fpd.sex              = smartcard::findString(fields, protocol::TAG_SEX);
    fpd.placeOfBirth     = smartcard::findString(fields, protocol::TAG_PLACE_OF_BIRTH);
    fpd.communityOfBirth = smartcard::findString(fields, protocol::TAG_COMMUNITY_OF_BIRTH);
    fpd.stateOfBirth     = smartcard::findString(fields, protocol::TAG_STATE_OF_BIRTH);
    fpd.dateOfBirth      = smartcard::findString(fields, protocol::TAG_DATE_OF_BIRTH);
    fpd.nationalityFull  = smartcard::findString(fields, protocol::TAG_NATIONALITY_FULL);
    fpd.statusOfForeigner = smartcard::findString(fields, protocol::TAG_STATUS_OF_FOREIGNER);
    return fpd;
}

VariablePersonalData EIdCard::readVariablePersonalData()
{
    auto raw = cardReader_->readFile(*connection_,
                                     protocol::FILE_VARIABLE_DATA_H,
                                     protocol::FILE_VARIABLE_DATA_L);
    auto fields = smartcard::parseTLV(raw.data(), raw.size());

    VariablePersonalData vpd;
    vpd.state           = smartcard::findString(fields, protocol::TAG_STATE);
    vpd.community       = smartcard::findString(fields, protocol::TAG_COMMUNITY);
    vpd.place           = smartcard::findString(fields, protocol::TAG_PLACE);
    vpd.street          = smartcard::findString(fields, protocol::TAG_STREET);
    vpd.houseNumber     = smartcard::findString(fields, protocol::TAG_HOUSE_NUMBER);
    vpd.houseLetter     = smartcard::findString(fields, protocol::TAG_HOUSE_LETTER);
    vpd.entrance        = smartcard::findString(fields, protocol::TAG_ENTRANCE);
    vpd.floor           = smartcard::findString(fields, protocol::TAG_FLOOR);
    vpd.apartmentNumber = smartcard::findString(fields, protocol::TAG_APARTMENT_NUMBER);
    vpd.addressDate     = smartcard::findString(fields, protocol::TAG_ADDRESS_DATE);
    vpd.addressLabel    = smartcard::findString(fields, protocol::TAG_ADDRESS_LABEL);
    return vpd;
}

PhotoData EIdCard::readPortrait()
{
    auto raw = cardReader_->readFile(*connection_,
                                     protocol::FILE_PORTRAIT_H,
                                     protocol::FILE_PORTRAIT_L);
    auto fields = smartcard::parseTLV(raw.data(), raw.size());

    return smartcard::findBytes(fields, protocol::TAG_PORTRAIT);
}

} // namespace eidcard
