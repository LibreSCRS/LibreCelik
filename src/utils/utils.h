// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#define DISABLE_COPY_MOVE(Class)                                                                                       \
    Class(const Class&) = delete;                                                                                      \
    Class& operator=(const Class&) = delete;                                                                           \
    Class(Class&&) = delete;                                                                                           \
    Class& operator=(Class&&) = delete
