// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef IPCSCSCANPROVIDER_H
#define IPCSCSCANPROVIDER_H

#ifdef __APPLE__
#include <PCSC/winscard.h>
#include <PCSC/wintypes.h>
#else
#include <winscard.h>
#endif

// Thin abstraction over the PC/SC functions used by SmartCardScanner.
// Production code uses PCSCScanProvider (real PC/SC); tests inject a mock.
class IPCSCScanProvider
{
public:
    virtual ~IPCSCScanProvider() = default;

    virtual LONG establishContext(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2,
                                  LPSCARDCONTEXT phContext) = 0;

    virtual LONG releaseContext(SCARDCONTEXT hContext) = 0;

    virtual LONG listReaders(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders) = 0;

    virtual LONG getStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout, SCARD_READERSTATE* rgReaderStates,
                                 DWORD cReaders) = 0;

    virtual LONG cancel(SCARDCONTEXT hContext) = 0;
};

#endif // IPCSCSCANPROVIDER_H
