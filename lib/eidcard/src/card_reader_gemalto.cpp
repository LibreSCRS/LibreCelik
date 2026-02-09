// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "card_reader_gemalto.h"
#include "card_protocol.h"
#include "smartcard/apdu.h"
#include "smartcard/pcsc_connection.h"
#include <stdexcept>

namespace eidcard {

CardType CardReaderGemalto::selectApplication(smartcard::PCSCConnection& conn)
{
    // Try SERID (citizen ID) first
    auto resp = conn.transmit(smartcard::selectByAID(protocol::AID_SERID));
    if (resp.isSuccess()) {
        return CardType::Gemalto2014;
    }

    // Try SERIF (foreigner ID)
    resp = conn.transmit(smartcard::selectByAID(protocol::AID_SERIF));
    if (resp.isSuccess()) {
        return CardType::ForeignerIF2020;
    }

    // Try SERRP (residence permit - also foreigner)
    resp = conn.transmit(smartcard::selectByAID(protocol::AID_SERRP));
    if (resp.isSuccess()) {
        return CardType::ForeignerIF2020;
    }

    return CardType::Unknown;
}

std::vector<uint8_t> CardReaderGemalto::readFile(smartcard::PCSCConnection& conn,
                                                  uint8_t fileId1, uint8_t fileId2)
{
    // SELECT file by path (P1=0x08)
    auto selectResp = conn.transmit(smartcard::selectByPath(fileId1, fileId2, 4));
    if (!selectResp.isSuccess()) {
        throw std::runtime_error("Gemalto: SELECT file failed, SW=" +
                                 std::to_string(selectResp.statusWord()));
    }

    // Read 4-byte header to get total file length
    // The header contains the file size in the SELECT response data
    // Response data format: file size as 2 bytes (big-endian) at offset 2-3
    uint32_t totalLength = 0;
    if (selectResp.data.size() >= 4) {
        totalLength = (static_cast<uint32_t>(selectResp.data[2]) << 8) |
                       static_cast<uint32_t>(selectResp.data[3]);
    } else {
        // Fallback: read header from binary
        auto headerResp = conn.transmit(smartcard::readBinary(0, 4));
        if (!headerResp.isSuccess() || headerResp.data.size() < 4) {
            throw std::runtime_error("Gemalto: Cannot read file header");
        }
        totalLength = (static_cast<uint32_t>(headerResp.data[2]) << 8) |
                       static_cast<uint32_t>(headerResp.data[3]);
    }

    if (totalLength == 0) {
        return {};
    }

    // Read the file content in chunks of 255 bytes
    std::vector<uint8_t> fileData;
    fileData.reserve(totalLength);
    uint16_t offset = 0;

    while (offset < totalLength) {
        uint8_t chunkSize = static_cast<uint8_t>(
            std::min(static_cast<uint32_t>(protocol::READ_CHUNK_SIZE),
                     totalLength - offset));

        auto readResp = conn.transmit(smartcard::readBinary(offset, chunkSize));
        if (!readResp.isSuccess()) {
            throw std::runtime_error("Gemalto: READ BINARY failed at offset " +
                                     std::to_string(offset));
        }

        fileData.insert(fileData.end(), readResp.data.begin(), readResp.data.end());
        offset += static_cast<uint16_t>(readResp.data.size());

        if (readResp.data.empty()) {
            break;  // No more data
        }
    }

    return fileData;
}

} // namespace eidcard
