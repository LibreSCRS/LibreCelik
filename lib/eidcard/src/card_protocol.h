// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDCARD_CARD_PROTOCOL_H
#define EIDCARD_CARD_PROTOCOL_H

#include <cstdint>
#include <vector>

namespace eidcard::protocol {

// Application Identifiers (AIDs) for Serbian eID cards
// SERID - Main eID application (citizen ID)
inline const std::vector<uint8_t> AID_SERID = {
    0xF3, 0x81, 0x00, 0x00, 0x02, 0x53, 0x45, 0x52, 0x49, 0x44, 0x01
};

// SERIF - Foreigner ID application
inline const std::vector<uint8_t> AID_SERIF = {
    0xF3, 0x81, 0x00, 0x00, 0x02, 0x53, 0x45, 0x52, 0x49, 0x46, 0x01
};

// SERRP - Residence Permit application
inline const std::vector<uint8_t> AID_SERRP = {
    0xF3, 0x81, 0x00, 0x00, 0x02, 0x53, 0x45, 0x52, 0x52, 0x50, 0x01
};

// File IDs (2 bytes each)
constexpr uint8_t FILE_DOCUMENT_DATA_H  = 0x0F;
constexpr uint8_t FILE_DOCUMENT_DATA_L  = 0x02;
constexpr uint8_t FILE_PERSONAL_DATA_H  = 0x0F;
constexpr uint8_t FILE_PERSONAL_DATA_L  = 0x03;
constexpr uint8_t FILE_VARIABLE_DATA_H  = 0x0F;
constexpr uint8_t FILE_VARIABLE_DATA_L  = 0x04;
constexpr uint8_t FILE_PORTRAIT_H       = 0x0F;
constexpr uint8_t FILE_PORTRAIT_L       = 0x06;

// TLV Tags for Document Data
constexpr uint16_t TAG_DOC_REG_NO           = 1546;
constexpr uint16_t TAG_DOCUMENT_TYPE        = 1547;
constexpr uint16_t TAG_DOCUMENT_SERIAL_NO   = 1548;
constexpr uint16_t TAG_ISSUING_DATE         = 1549;
constexpr uint16_t TAG_EXPIRY_DATE          = 1550;
constexpr uint16_t TAG_ISSUING_AUTHORITY    = 1551;
constexpr uint16_t TAG_CHIP_SERIAL_NUMBER   = 1689;

// TLV Tags for Fixed Personal Data
constexpr uint16_t TAG_PERSONAL_NUMBER      = 1558;
constexpr uint16_t TAG_SURNAME              = 1559;
constexpr uint16_t TAG_GIVEN_NAME           = 1560;
constexpr uint16_t TAG_PARENT_GIVEN_NAME    = 1561;
constexpr uint16_t TAG_SEX                  = 1562;
constexpr uint16_t TAG_PLACE_OF_BIRTH       = 1563;
constexpr uint16_t TAG_COMMUNITY_OF_BIRTH   = 1564;
constexpr uint16_t TAG_STATE_OF_BIRTH       = 1565;
constexpr uint16_t TAG_DATE_OF_BIRTH        = 1566;
constexpr uint16_t TAG_NATIONALITY_FULL     = 1583;
constexpr uint16_t TAG_STATUS_OF_FOREIGNER  = 1610;

// TLV Tags for Variable Personal Data
constexpr uint16_t TAG_STATE                = 1568;
constexpr uint16_t TAG_COMMUNITY            = 1569;
constexpr uint16_t TAG_PLACE                = 1570;
constexpr uint16_t TAG_STREET               = 1571;
constexpr uint16_t TAG_HOUSE_NUMBER         = 1572;
constexpr uint16_t TAG_HOUSE_LETTER         = 1573;
constexpr uint16_t TAG_ENTRANCE             = 1574;
constexpr uint16_t TAG_FLOOR                = 1575;
constexpr uint16_t TAG_APARTMENT_NUMBER     = 1578;
constexpr uint16_t TAG_ADDRESS_DATE         = 1580;
constexpr uint16_t TAG_ADDRESS_LABEL        = 1581;

// TLV Tags for Portrait
constexpr uint16_t TAG_PORTRAIT             = 1584;

// ATR patterns for card type detection
// Gemalto (2014+) cards have ATR starting with 3B FF 94 00 00
// Apollo (pre-2014) cards have ATR starting with 3B B9 18 00
inline bool isGemaltoATR(const std::vector<uint8_t>& atr)
{
    return atr.size() >= 5 &&
           atr[0] == 0x3B &&
           atr[1] == 0xFF &&
           atr[2] == 0x94;
}

inline bool isApolloATR(const std::vector<uint8_t>& atr)
{
    return atr.size() >= 4 &&
           atr[0] == 0x3B &&
           atr[1] == 0xB9 &&
           atr[2] == 0x18;
}

// Read chunk size
constexpr uint8_t READ_CHUNK_SIZE = 0xFF;  // 255 bytes per READ BINARY

} // namespace eidcard::protocol

#endif // EIDCARD_CARD_PROTOCOL_H
