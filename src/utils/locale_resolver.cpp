// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/locale_resolver.h"

#include <QLocale>

namespace utils {

QStringList supportedLocaleCodes()
{
    // Keep in sync with the .ts → .qm pipeline in src/CMakeLists.txt and
    // the language dropdown in SettingsDialog.
    return {QStringLiteral("en"), QStringLiteral("sr_RS")};
}

QString resolveActiveLocale(const QString& userPreference, const QStringList& supportedCodes,
                            const QStringList& systemUiLanguages)
{
    // 1. User's explicit preference, if we ship a translation for it.
    if (!userPreference.isEmpty() && supportedCodes.contains(userPreference)) {
        return userPreference;
    }

    // 2. Walk the platform's UI-language preference list (most preferred
    //    first). Each tag (e.g. "sr-RS", "en-US") is normalised through
    //    QLocale::name() to the underscore form QSettings/QTranslator use
    //    (e.g. "sr_RS"); the first that exactly matches a supported code
    //    wins.
    for (const QString& tag : systemUiLanguages) {
        const QString name = QLocale(tag).name();
        if (supportedCodes.contains(name)) {
            return name;
        }
    }

    // 3. Hard default: English. Always present in supportedCodes per the
    //    project's i18n discipline (every release ships en + at least one
    //    other) but guarded explicitly to keep the contract tight.
    static const QString en = QStringLiteral("en");
    if (supportedCodes.contains(en)) {
        return en;
    }

    // 4. Pathological fallback (shouldn't happen — see step 3 invariant).
    return supportedCodes.value(0);
}

} // namespace utils
