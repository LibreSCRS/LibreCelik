// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "card_reader_apollo.h"
#include "card_protocol.h"
#include "smartcard/apdu.h"
#include "smartcard/pcsc_connection.h"
#include <stdexcept>

namespace eidcard {

std::vector<uint8_t> CardReaderApollo::readFile(smartcard::PCSCConnection& conn,
                                                 uint8_t fileId1, uint8_t fileId2)
{
    // Apollo cards: SELECT by file ID (P1=0x00)
    auto selectResp = conn.transmit(smartcard::selectByFileId(fileId1, fileId2));
    // Apollo may return 0x61XX (more data available) or 0x9000
    if (selectResp.sw1 != 0x90 && selectResp.sw1 != 0x61) {
        throw std::runtime_error("Apollo: SELECT file failed, SW=" +
                                 std::to_string(selectResp.statusWord()));
    }

    // Read 6-byte header to get total file length
    auto headerResp = conn.transmit(smartcard::readBinary(0, 6));
    if (!headerResp.isSuccess() || headerResp.data.size() < 6) {
        throw std::runtime_error("Apollo: Cannot read file header");
    }

    // Check for empty file marker (0xFF at offset 4)
    if (headerResp.data[4] == 0xFF) {
        return {};
    }

    // Total length from header bytes [2..3] (big-endian) + 2 header bytes
    uint32_t totalLength = (static_cast<uint32_t>(headerResp.data[2]) << 8) |
                            static_cast<uint32_t>(headerResp.data[3]);

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
            throw std::runtime_error("Apollo: READ BINARY failed at offset " +
                                     std::to_string(offset));
        }

        fileData.insert(fileData.end(), readResp.data.begin(), readResp.data.end());
        offset += static_cast<uint16_t>(readResp.data.size());

        if (readResp.data.empty()) {
            break;
        }
    }

    return fileData;
}

} // namespace eidcard
