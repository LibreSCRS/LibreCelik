// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief gtest base fixture for runtime language-switch (retranslate)
///        regression tests.
///
/// `RetranslatableWidgetFixture` installs the LibreCelik en + sr_RS
/// translators, walks any `QWidget*` subtree, and snapshots every
/// translatable attribute. The fixture is reused by D7
/// (preference-state) and D9 (plugin retranslate) test suites.
///
/// Usage:
/// @code
///   class MyTest : public RetranslatableWidgetFixture {};
///
///   TEST_F(MyTest, allLabelsRetranslate)
///   {
///       auto* widget = new MyWidget(...);
///       widget->show();
///       auto before = snapshotTexts(widget);
///       switchTo(QStringLiteral("sr_RS"));
///       QCoreApplication::processEvents();
///       auto after = snapshotTexts(widget);
///       assertAllRetranslated(before, after);
///   }
/// @endcode
///
/// All tests run with `QT_QPA_PLATFORM=offscreen` per
/// `feedback_qt_offscreen_for_tests.md` (gtest properties wired in
/// CMake). The fixture removes its own translators in `TearDown` so
/// state does not leak between tests.

#pragma once

#include <QApplication>
#include <QMap>
#include <QString>
#include <QTranslator>
#include <gtest/gtest.h>

class QWidget;

namespace librecelik::test::i18n {

class RetranslatableWidgetFixture : public ::testing::Test
{
protected:
    void SetUp() override;
    void TearDown() override;

    /// @brief Switch the application UI language.
    /// @param languageCode e.g. "en" or "sr_RS". Removes the previously
    /// installed translator and installs the requested one.
    void switchTo(const QString& languageCode);

    /// @brief Walk @p root via depth-first traversal and snapshot every
    /// translatable attribute keyed by `<objectNameOrPath>:<role>`.
    [[nodiscard]] QMap<QString, QString> snapshotTexts(QWidget* root) const;

    /// @brief Verify every (key in @p before AND @p after) entry has a
    /// different value, except those silenced by:
    ///   - locale-stable wordlist (PIN, PIV, OK, …);
    ///   - technical-value heuristic (digits, ISO date, URL, …);
    ///   - widget Q_PROPERTY @c i18n-locale-stable;
    ///   - widget Q_PROPERTY @c i18n-non-translatable.
    /// Uses gtest ADD_FAILURE/EXPECT_NE so multiple mismatches are
    /// surfaced in a single test run.
    void assertAllRetranslated(const QMap<QString, QString>& before, const QMap<QString, QString>& after) const;

    /// @brief Return a copy of @p root flagged as locale-stable via
    /// Q_PROPERTY (test helper for self-tests).
    static void markLocaleStable(QWidget* w);

    /// @brief Return the path to LibreCelik translation .qm files
    /// resolved at test-binary launch.
    [[nodiscard]] static QString translationsDir();

private:
    QTranslator m_translator;
    QString m_currentLanguage;
};

} // namespace librecelik::test::i18n
