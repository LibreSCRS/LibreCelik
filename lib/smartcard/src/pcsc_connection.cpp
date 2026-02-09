// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "smartcard/pcsc_connection.h"
#include <cstring>

namespace smartcard {

PCSCConnection::PCSCConnection(const std::string& readerName)
{
    LONG rv = SCardEstablishContext(SCARD_SCOPE_SYSTEM, nullptr, nullptr, &context_);
    if (rv != SCARD_S_SUCCESS) {
        throw PCSCError("SCardEstablishContext failed", rv);
    }

    rv = SCardConnect(context_,
                      readerName.c_str(),
                      SCARD_SHARE_SHARED,
                      SCARD_PROTOCOL_T0 | SCARD_PROTOCOL_T1,
                      &card_,
                      &activeProtocol_);
    if (rv != SCARD_S_SUCCESS) {
        SCardReleaseContext(context_);
        throw PCSCError("SCardConnect failed on reader: " + readerName, rv);
    }
}

PCSCConnection::~PCSCConnection()
{
    if (card_) {
        SCardDisconnect(card_, SCARD_LEAVE_CARD);
    }
    if (context_) {
        SCardReleaseContext(context_);
    }
}

APDUResponse PCSCConnection::transmit(const APDUCommand& cmd)
{
    auto cmdBytes = cmd.toBytes();

    const SCARD_IO_REQUEST* pioSendPci =
        (activeProtocol_ == SCARD_PROTOCOL_T0) ? SCARD_PCI_T0 : SCARD_PCI_T1;

    uint8_t recvBuffer[258];
    DWORD recvLength = sizeof(recvBuffer);

    LONG rv = SCardTransmit(card_,
                            pioSendPci,
                            cmdBytes.data(),
                            static_cast<DWORD>(cmdBytes.size()),
                            nullptr,
                            recvBuffer,
                            &recvLength);
    if (rv != SCARD_S_SUCCESS) {
        throw PCSCError("SCardTransmit failed", rv);
    }

    APDUResponse response;
    if (recvLength >= 2) {
        response.sw1 = recvBuffer[recvLength - 2];
        response.sw2 = recvBuffer[recvLength - 1];
        if (recvLength > 2) {
            response.data.assign(recvBuffer, recvBuffer + recvLength - 2);
        }
    }

    return response;
}

std::vector<uint8_t> PCSCConnection::getATR() const
{
    DWORD readerLen = 0;
    DWORD state = 0;
    DWORD protocol = 0;
    BYTE atr[MAX_ATR_SIZE];
    DWORD atrLen = sizeof(atr);

    LONG rv = SCardStatus(card_, nullptr, &readerLen, &state, &protocol, atr, &atrLen);
    if (rv != SCARD_S_SUCCESS) {
        throw PCSCError("SCardStatus failed", rv);
    }

    return std::vector<uint8_t>(atr, atr + atrLen);
}

} // namespace smartcard
