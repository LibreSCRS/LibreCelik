// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "utils/localeresolver.h"

#include <QLocale>

namespace utils {

QStringList supportedLocaleCodes()
{
    // Keep in sync with the .ts → .qm pipeline in src/CMakeLists.txt and
    // the language dropdown in SettingsDialog.
    //
    // "sr_Latn_RS" is the Serbian Cyrillic catalogue transliterated to
    // Latin script. The exact code is load-bearing, not cosmetic: measured
    // against lrelease6 (Qt 6.11.1), the gettext/POSIX modifier spelling
    // ("sr@latin" — correct for, and used as-is by, LibreKDE's separate
    // gettext/po pipeline) makes lrelease treat this as numerus-incompatible
    // with "sr_RS" and silently collapse Serbian's three plural forms down
    // to one ("Removed plural forms as the target language has less
    // forms."), which breaks plural rendering (e.g. n=2 would read
    // "2 potvrda" instead of the correct "2 potvrde"). The BCP47-style
    // script tag "sr_Latn_RS" keeps lrelease at three forms, matching
    // Serbian's actual plural rule, in the Qt .ts/.qm chain this project
    // uses.
    //
    // Cyrillic ("sr_RS") remains the primary/default Serbian rendering.
    // Latin is reachable only via an explicit dropdown choice or a
    // persisted kLanguage preference — never via the automatic
    // system-locale walk in resolveActiveLocale() below, because
    // QLocale::name() always drops the script segment (measured on Qt
    // 6.11.1: "sr-Latn-RS" → "sr_RS"), so a Latin-preferring system locale
    // still resolves to the Cyrillic code here. Pinned by
    // LocaleResolverTest.CyrillicPrimaryOverSystemLatinPreference.
    return {QStringLiteral("en"), QStringLiteral("sr_RS"), QStringLiteral("sr_Latn_RS")};
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
