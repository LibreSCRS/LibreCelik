// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief An anonymous, writable, seekable descriptor for tests.
///
/// Linux has `memfd_create`; macOS has nothing like it. The cases that use this
/// need only what the name promises -- a descriptor with no directory entry
/// that dies with its last reference -- and an immediately-unlinked temporary
/// file is exactly that.
///
/// NOT a substitute where SEALING matters. A sealed memfd has no portable
/// equivalent, and `readBoundedPayload` refuses an unsealed descriptor by
/// design, so the cases that need one stay behind a Linux guard of their own.

#pragma once

#include <unistd.h>

#if defined(__linux__)
#include <sys/mman.h>
#else
#include <cstdlib>
#endif

namespace librecelik::test {

/// An anonymous writable descriptor, or -1. @p name is the memfd label on
/// Linux and is unused elsewhere.
[[nodiscard]] inline int makeAnonymousFd(const char* name)
{
#if defined(__linux__)
    return ::memfd_create(name, 0);
#else
    (void)name;
    char pattern[] = "/tmp/librecelik-anonfd-XXXXXX";
    const int fd = ::mkstemp(pattern);
    if (fd >= 0) {
        ::unlink(pattern); // no directory entry from here on, as on Linux
    }
    return fd;
#endif
}

} // namespace librecelik::test
