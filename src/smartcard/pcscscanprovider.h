// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef PCSCSCANPROVIDER_H
#define PCSCSCANPROVIDER_H

#include "ipcscscanprovider.h"

// Real PC/SC implementation — delegates directly to libpcsclite.
class PCSCScanProvider : public IPCSCScanProvider
{
public:
    LONG establishContext(DWORD dwScope, LPCVOID pvReserved1, LPCVOID pvReserved2, LPSCARDCONTEXT phContext) override
    {
        return SCardEstablishContext(dwScope, pvReserved1, pvReserved2, phContext);
    }

    LONG releaseContext(SCARDCONTEXT hContext) override
    {
        return SCardReleaseContext(hContext);
    }

    LONG listReaders(SCARDCONTEXT hContext, LPCSTR mszGroups, LPSTR mszReaders, LPDWORD pcchReaders) override
    {
        return SCardListReaders(hContext, mszGroups, mszReaders, pcchReaders);
    }

    LONG getStatusChange(SCARDCONTEXT hContext, DWORD dwTimeout, SCARD_READERSTATE* rgReaderStates,
                         DWORD cReaders) override
    {
        return SCardGetStatusChange(hContext, dwTimeout, rgReaderStates, cReaders);
    }

    LONG cancel(SCARDCONTEXT hContext) override
    {
        return SCardCancel(hContext);
    }
};

#endif // PCSCSCANPROVIDER_H
