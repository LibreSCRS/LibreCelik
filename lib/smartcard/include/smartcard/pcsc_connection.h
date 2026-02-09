// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARD_PCSC_CONNECTION_H
#define SMARTCARD_PCSC_CONNECTION_H

#include <string>
#include <vector>
#include <stdexcept>

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif

#include "apdu.h"

namespace smartcard {

class PCSCError : public std::runtime_error {
public:
    PCSCError(const std::string& msg, LONG code)
        : std::runtime_error(msg), code_(code) {}
    LONG code() const { return code_; }
private:
    LONG code_;
};

class PCSCConnection {
public:
    explicit PCSCConnection(const std::string& readerName);
    ~PCSCConnection();

    PCSCConnection(const PCSCConnection&) = delete;
    PCSCConnection& operator=(const PCSCConnection&) = delete;

    APDUResponse transmit(const APDUCommand& cmd);
    std::vector<uint8_t> getATR() const;

private:
    SCARDCONTEXT context_ = 0;
    SCARDHANDLE card_ = 0;
    DWORD activeProtocol_ = 0;
};

} // namespace smartcard

#endif // SMARTCARD_PCSC_CONNECTION_H
