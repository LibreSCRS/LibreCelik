// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef EIDCARD_CARD_READER_BASE_H
#define EIDCARD_CARD_READER_BASE_H

#include <cstdint>
#include <vector>

namespace smartcard {
class PCSCConnection;
}

namespace eidcard {

class CardReaderBase {
public:
    virtual ~CardReaderBase() = default;
    virtual std::vector<uint8_t> readFile(smartcard::PCSCConnection& conn,
                                          uint8_t fileId1, uint8_t fileId2) = 0;
};

} // namespace eidcard

#endif // EIDCARD_CARD_READER_BASE_H
