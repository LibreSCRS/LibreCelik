// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef SMARTCARDWRAPPER_H
#define SMARTCARDWRAPPER_H
#include <PCSC/wintypes.h>
#include <PCSC/winscard.h>
#include <string>

inline std::string buildErrorMessage(const char* callerFunctionName, const char* scardFunctionName,
                                     const LONG result, const char* file, int line)
{
    return std::string(scardFunctionName) + " returned " + std::to_string(result) + " in "
           + file + ':' + std::to_string(line) + ':' + callerFunctionName;
}

template <typename Func, typename... Args>
void SmartCardWrapper(const char* callerFunctionName, const char* file, int line,
               const char* scardFunctionName, Func scardFunction, Args... args)
{
    const LONG result = scardFunction(args...);

    switch (result) {
    case SCARD_S_SUCCESS:
        return;
    case LONG(SCARD_E_NO_SERVICE):
    case LONG(SCARD_E_SERVICE_STOPPED):
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    case LONG(SCARD_E_NO_READERS_AVAILABLE):
    case LONG(SCARD_E_READER_UNAVAILABLE):
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    case LONG(SCARD_E_NO_SMARTCARD):
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    case LONG(SCARD_E_NOT_READY):
    case LONG(SCARD_E_INVALID_VALUE):
    case LONG(SCARD_E_COMM_DATA_LOST):
    case LONG(SCARD_W_RESET_CARD):
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    case LONG(SCARD_W_REMOVED_CARD):
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    case LONG(SCARD_E_NOT_TRANSACTED):
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    default:
        throw std::runtime_error(buildErrorMessage(callerFunctionName, scardFunctionName, result, file, line));
    }
}

#define SCard(APIFunctionName, ...)                                                                \
SmartCardWrapper(__FUNCTION__, __FILE__, __LINE__, "SCard" #APIFunctionName, SCard##APIFunctionName, __VA_ARGS__)

#endif // SMARTCARDWRAPPER_H
