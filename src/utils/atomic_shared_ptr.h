// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <memory>
#include <mutex>
#include <utility>

/// @file
/// @brief Drop-in replacement for @c std::atomic<std::shared_ptr<T>>.
///
/// libc++ has NOT implemented C++20 P0718R2 (Atomic shared_ptr) as of
/// LLVM 19 / Apple Clang 16; tracked in
/// https://github.com/llvm/llvm-project/issues/99980. libstdc++ and MSVC
/// implement it, but their implementations use an internal mutex anyway —
/// so this wrapper has identical performance characteristics on all
/// platforms and lets the LC source compile uniformly.
///
/// @warning Replace every @c librecelik::detail::AtomicSharedPtr<T> with
///          @c std::atomic<std::shared_ptr<T>> once libc++ closes
///          llvm/llvm-project#99980 and Apple Clang ships a libc++ snapshot
///          containing the fix.

namespace librecelik::detail {

template <typename T>
class AtomicSharedPtr
{
public:
    [[nodiscard]] std::shared_ptr<T> load() const
    {
        std::lock_guard lock(mtx);
        return ptr;
    }

    void store(std::shared_ptr<T> v)
    {
        std::lock_guard lock(mtx);
        ptr = std::move(v);
    }

private:
    mutable std::mutex mtx;
    std::shared_ptr<T> ptr;
};

} // namespace librecelik::detail
