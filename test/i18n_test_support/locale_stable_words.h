// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Locale-stable wordlist + technical-value heuristic for the
///        i18n retranslate runtime fixture.
///
/// When `RetranslatableWidgetFixture::assertAllRetranslated()` finds a
/// widget attribute whose value is identical before and after a
/// language switch, it skips the failure if the value:
///
///   1. matches an entry in @ref localeStableWords (e.g. "PIN", "OK",
///      "PIV") — these read the same in en and sr_RS by convention; or
///   2. matches the technical-value heuristic (pure digits, ISO date,
///      URL, single character, empty string).
///
/// Per spec §7.2 (knowledge/specs/2026-05-08-i18n-retranslate-audit-design.md),
/// the wordlist is the *first-line* filter to keep tests from being
/// noisy. Adding a term requires PR review (see
/// CONTRIBUTING.md "Locale-stable wordlist amendments").

#pragma once

#include <QSet>
#include <QString>
#include <QStringView>

namespace librecelik::test::i18n {

/// @brief Set of strings that read identically in en and sr_RS by
/// convention (acronyms, technical identifiers).
///
/// Initial population per spec §7.2:
///   PIN, PUK, PIV, eID, MRZ, BAC, PACE, OK, RFID, NFC, ICAO, EU,
///   PKCS, CRL, OCSP, TLS, URL, ID, USB.
///
/// Plus Qt-built-in dialog button labels and tab-bar control strings
/// that come from QtBase translations (qt_*.qm), not from
/// LibreCelik_*.qm. In a CI environment that does not bundle the Qt
/// system translations, these stay in English regardless of our
/// language switch — which is fine: they are still translated *by
/// Qt itself* when its translations are present, and our retranslate
/// audit only owns LC-side strings.
inline const QSet<QString>& localeStableWords()
{
    static const QSet<QString> kWords{
        QStringLiteral("PIN"),
        QStringLiteral("PUK"),
        QStringLiteral("PIV"),
        QStringLiteral("eID"),
        QStringLiteral("MRZ"),
        QStringLiteral("BAC"),
        QStringLiteral("PACE"),
        QStringLiteral("OK"),
        QStringLiteral("RFID"),
        QStringLiteral("NFC"),
        QStringLiteral("ICAO"),
        QStringLiteral("EU"),
        QStringLiteral("PKCS"),
        QStringLiteral("CRL"),
        QStringLiteral("OCSP"),
        QStringLiteral("TLS"),
        QStringLiteral("URL"),
        QStringLiteral("ID"),
        QStringLiteral("USB"),
        // Qt dialog button labels — translated by qt_*.qm, not by us.
        QStringLiteral("Cancel"),
        QStringLiteral("Apply"),
        QStringLiteral("Reset"),
        QStringLiteral("Help"),
        QStringLiteral("Discard"),
        QStringLiteral("Close"),
        // Qt tab-widget scroll-button accessible names.
        QStringLiteral("Scroll Left"),
        QStringLiteral("Scroll Right"),
        // Endonyms — intentionally locale-stable (a Serbian user expects
        // to see "English" labelling the English option, and a French
        // user likewise expects "Српски" to label Serbian). The
        // SettingsDialog language combo uses literal QStringLiterals.
        QStringLiteral("English"),
        QStringLiteral("Српски"),
    };
    return kWords;
}

/// @brief Return true when @p v is a technical (non-translated) value.
///
/// Heuristic matches:
///   - empty string,
///   - single character,
///   - pure ASCII digits (e.g. "12345"),
///   - ISO 8601 date (YYYY-MM-DD),
///   - URL (starts with "http://" or "https://").
[[nodiscard]] inline bool isTechnicalValue(QStringView v)
{
    if (v.isEmpty())
        return true;
    if (v.size() == 1)
        return true;
    if (v.startsWith(QLatin1String("http://")) || v.startsWith(QLatin1String("https://")))
        return true;
    // ISO date: YYYY-MM-DD
    if (v.size() == 10 && v[4] == QLatin1Char('-') && v[7] == QLatin1Char('-')) {
        bool digits = true;
        for (qsizetype i = 0; i < v.size(); ++i) {
            if (i == 4 || i == 7)
                continue;
            if (!v[i].isDigit()) {
                digits = false;
                break;
            }
        }
        if (digits)
            return true;
    }
    // pure digits
    bool allDigits = true;
    for (auto ch : v) {
        if (!ch.isDigit()) {
            allDigits = false;
            break;
        }
    }
    if (allDigits)
        return true;
    return false;
}

/// @brief Q_PROPERTY name to mark a widget whose value is intentionally
/// non-translatable (e.g. a serial number display).
constexpr const char* kPropI18nNonTranslatable = "i18n-non-translatable";

/// @brief Q_PROPERTY name to mark a widget whose translation is
/// intentionally locale-stable (same text in every language).
constexpr const char* kPropI18nLocaleStable = "i18n-locale-stable";

} // namespace librecelik::test::i18n
