// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#ifndef UTILS_H
#define UTILS_H

#define DISABLE_COPY_MOVE(Class)                                                          \
Class(const Class&) = delete;                                                                  \
    Class& operator=(const Class&) = delete;                                                       \
    Class(Class&&) = delete;                                                                       \
    Class& operator=(Class&&) = delete

#endif // UTILS_H
