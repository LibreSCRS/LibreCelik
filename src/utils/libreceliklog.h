// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once

#include <QDebug>
#include <QLoggingCategory>
#include <string>

// Qt 6.7+ adds QDebug::operator<<(std::string) as a member; older versions are
// ambiguous (std::string matches both QUtf8StringView and QByteArrayView).
#if QT_VERSION < QT_VERSION_CHECK(6, 7, 0)
inline QDebug operator<<(QDebug dbg, const std::string& s)
{
    return dbg << QString::fromStdString(s);
}
#endif

Q_DECLARE_LOGGING_CATEGORY(libreSCRSGeneral)
Q_DECLARE_LOGGING_CATEGORY(librecSCRSCard)
Q_DECLARE_LOGGING_CATEGORY(libreCelikAPI)
Q_DECLARE_LOGGING_CATEGORY(libreCelikPrinting)
Q_DECLARE_LOGGING_CATEGORY(libreCelikCertificates)
Q_DECLARE_LOGGING_CATEGORY(libreCelikHealth)
Q_DECLARE_LOGGING_CATEGORY(libreCelikPks)
