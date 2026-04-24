// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "pkcs11utils.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace signing {

QString findPkcs11Module()
{
#ifdef __APPLE__
    const QString moduleName = QStringLiteral("librescrs-pkcs11.dylib");
#else
    const QString moduleName = QStringLiteral("librescrs-pkcs11.so");
#endif
    const QDir appDir(QCoreApplication::applicationDirPath());

    const QStringList searchPaths = {
        appDir.filePath(moduleName),
        appDir.filePath(QStringLiteral("../lib/") + moduleName),
        appDir.filePath(QStringLiteral("../lib/pkcs11/") + moduleName),
        appDir.filePath(QStringLiteral("../Frameworks/") + moduleName),
        appDir.filePath(QStringLiteral("../_deps/libremiddleware-build/lib/pkcs11/") + moduleName),
        appDir.filePath(QStringLiteral("../../LibreMiddleware/build/lib/pkcs11/") + moduleName),
#ifdef _WIN32
        appDir.filePath(QStringLiteral("librescrs-pkcs11.dll")),
#endif
    };

    for (const auto& path : searchPaths) {
        if (QFile::exists(path))
            return QFileInfo(path).canonicalFilePath();
    }

    return moduleName;
}

} // namespace signing
