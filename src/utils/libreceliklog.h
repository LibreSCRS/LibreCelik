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

// LibreCelik logging categories. The C++ identifiers use a short `lc*` prefix;
// the runtime category strings (`rs.libresc.librecelik.<subsystem>`) are the
// stable external contract — they are what users filter on via
// QT_LOGGING_RULES and what appears in journald output.
Q_DECLARE_LOGGING_CATEGORY(lcGeneral)
Q_DECLARE_LOGGING_CATEGORY(lcSmartCard)
Q_DECLARE_LOGGING_CATEGORY(lcPrinting)
Q_DECLARE_LOGGING_CATEGORY(lcCertificates)
Q_DECLARE_LOGGING_CATEGORY(lcPluginRegistry)
