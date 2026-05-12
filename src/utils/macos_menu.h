// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QtGlobal>

#ifdef Q_OS_MACOS
#include <QString>

struct MacOsAppMenuStrings
{
    QString about;
    QString preferences;
    QString services;
    QString hide;
    QString hideOthers;
    QString showAll;
    QString quit;
};

void macosRetranslateAppMenu(const MacOsAppMenuStrings& strings);
#endif
