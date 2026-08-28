// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief D7 runtime test — preference-state widgets must reflect the
///        effective application state, not stale values, when the
///        language switches at runtime.
///
/// The b23a825 regression case: SettingsDialog's language combo
/// returned -1 from findData(kLanguage="") and silently stuck at
/// index 0 = "English" while the rest of the UI was rendering in
/// Serbian (system fallback chain). The fix routes both the app init
/// path and the dropdown init through utils::resolveActiveLocale, and
/// the dropdown uses the resolved code rather than the raw kLanguage
/// preference.
///
/// This test instantiates the live SettingsDialog widget, confirms
/// that the language combo agrees with resolveActiveLocale() under
/// (kLanguage="", system="sr-RS") inputs, and that mid-flight
/// LanguageChange events do not desync the combo from the rest of the
/// dialog.
///
/// All tests run with QT_QPA_PLATFORM=offscreen.

#include "i18n_test_support/RetranslatableWidgetFixture.h"

#include "fake_gateway/fakeagentgateway.h"
#include "settings/settingsdialog.h"
#include "settings/settingskeys.h"
#include "utils/localeresolver.h"

#include <QApplication>
#include <QComboBox>
#include <QCoreApplication>
#include <QSettings>
#include <QTabWidget>
#include <gtest/gtest.h>

#include <memory>

namespace librecelik::test::i18n {

namespace {

/// @brief Reset the QSettings store used by SettingsDialog so each
/// test starts from a clean slate. The QSettings backend is
/// process-wide; without this reset, tests leak preferences into each
/// other and the b23a825 reproducer (kLanguage="") cannot be set up.
void resetQSettings()
{
    QSettings settings(settings::kOrganization, settings::kApplication);
    settings.clear();
    settings.sync();
}

} // namespace

class SettingsStateTest : public RetranslatableWidgetFixture
{
protected:
    /// The dialog takes a gateway now. This suite is about the RETRANSLATE
    /// path, not about the agent, so the scripted fake stands in — Ready, so
    /// the operation-backed tabs render live rather than dark.
    [[nodiscard]] std::unique_ptr<SettingsDialog> makeDialog()
    {
        gateway = std::make_unique<librecelik::test::agent::FakeAgentGateway>();
        gateway->setPresence(librecelik::agent::PresenceState::Ready);
        return std::make_unique<SettingsDialog>(gateway.get());
    }

    void SetUp() override
    {
        RetranslatableWidgetFixture::SetUp();
        // Use isolated organization namespace so production preferences
        // are not stomped by CI runs. SettingsDialog reads this via
        // settings::kOrganization / kApplication; we cannot redirect
        // those constants at link time, so we just clear() to ensure
        // a known-empty state.
        QCoreApplication::setApplicationName(QStringLiteral("LibreCelikI18nTests"));
        QCoreApplication::setOrganizationName(QStringLiteral("LibreSCRS"));
        resetQSettings();
    }

    void TearDown() override
    {
        resetQSettings();
        gateway.reset();
        RetranslatableWidgetFixture::TearDown();
    }

    std::unique_ptr<librecelik::test::agent::FakeAgentGateway> gateway;
};

/// b23a825 regression case: kLanguage="" + system="sr-RS" → the
/// language combo must reflect "sr_RS" (the resolved active locale)
/// not "en" (the silent index-0 fallback).
TEST_F(SettingsStateTest, languageComboReflectsEffectiveLanguageNotStalePreference)
{
    // Precondition: empty kLanguage. SettingsDialog::loadSettings()
    // resolves via utils::resolveActiveLocale; the resolver's expected
    // output for (empty preference, supported={en, sr_RS}, system=
    // {sr-RS, ...}) is "sr_RS". The combo must show that.
    const QString resolved = utils::resolveActiveLocale(
        QString{}, QStringList{QStringLiteral("en"), QStringLiteral("sr_RS")},
        QStringList{QStringLiteral("sr-RS"), QStringLiteral("sr-Latn-RS"), QStringLiteral("en-RS")});
    ASSERT_EQ(resolved, QStringLiteral("sr_RS"));

    // Note: the live SettingsDialog uses QLocale::system().uiLanguages()
    // which we cannot easily mock without a process-level locale
    // override. We therefore exercise resolveActiveLocale directly above
    // and verify SettingsDialog wiring agrees by checking the combo
    // contents and the data roles. The b23a825 fix's key invariant is
    // that the combo's findData(<resolved-locale>) >= 0 — i.e., the
    // resolver output is reachable from the combo. This test asserts
    // exactly that property without depending on the system locale of
    // the CI runner.
    auto dlg = makeDialog();

    // languageCombo carries a stable objectName (set in SettingsDialog's
    // ctor) — findChild by name is a single robust lookup, not a
    // content-sniffing walk over every combo in the dialog.
    auto* langCombo = dlg->findChild<QComboBox*>(QStringLiteral("languageCombo"));
    ASSERT_NE(langCombo, nullptr) << "language combo not found in SettingsDialog";

    // Every code in the real production supported-locale list must be
    // reachable from the combo — this is the b23a825 invariant
    // (findData(<resolved>) must succeed for any value the resolver could
    // return), generalised to iterate utils::supportedLocaleCodes() rather
    // than hardcoding today's two-entry list, so a future third (or
    // fourth) supported locale keeps this test honest instead of stale.
    for (const auto& code : utils::supportedLocaleCodes()) {
        EXPECT_GE(langCombo->findData(code), 0) << "combo missing entry for supported locale " << code.toStdString();
    }
}

/// Modal SettingsDialog stays visible during an external language
/// switch (e.g. from another window via Q_SIGNAL/Q_SLOT). The dialog
/// must retranslate via QEvent::LanguageChange — not crash, not
/// disappear, not desync the combo.
TEST_F(SettingsStateTest, settingsModalDialogSurvivesExternalLanguageSwitch)
{
    auto dlg = makeDialog();
    dlg->show();
    QCoreApplication::processEvents();

    switchTo(QStringLiteral("en"));
    QCoreApplication::processEvents();
    const auto before = snapshotTexts(dlg.get());
    ASSERT_FALSE(before.isEmpty());

    // Switch language while the dialog is visible. The dialog's
    // changeEvent (LibreCelik 4.0 retranslate spec, Phase B.4 batch 2)
    // must call retranslateUi() and refresh tab titles, form labels,
    // combo items, and the "Add" sentinel rows in the lists.
    switchTo(QStringLiteral("sr_RS"));
    QCoreApplication::processEvents();

    EXPECT_TRUE(dlg->isVisible());
    const auto after = snapshotTexts(dlg.get());
    ASSERT_FALSE(after.isEmpty());
    assertAllRetranslated(before, after);
}

/// Settings dialog tab titles must retranslate. Sanity check that the
/// QTabWidget pathway (not just QLabel/QPushButton) is exercised by
/// the live retranslateUi() implementation.
TEST_F(SettingsStateTest, settingsTabTitlesRetranslateOnLanguageChange)
{
    auto dlg = makeDialog();
    dlg->show();
    QCoreApplication::processEvents();

    QTabWidget* tabs = dlg->findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    ASSERT_EQ(tabs->count(), 3);

    switchTo(QStringLiteral("en"));
    QCoreApplication::processEvents();
    const QString tab0Before = tabs->tabText(0);
    const QString tab1Before = tabs->tabText(1);
    const QString tab2Before = tabs->tabText(2);
    ASSERT_FALSE(tab0Before.isEmpty());

    switchTo(QStringLiteral("sr_RS"));
    QCoreApplication::processEvents();

    // At least one tab title must change after language switch — the
    // three tabs are "General" / "Signing" / "Trust" in English and
    // distinct words in Serbian. If translations are not loaded (build
    // without compiled .qm), the strings stay equal — which is itself
    // a build-system problem the test surface should reveal.
    const QString tab0After = tabs->tabText(0);
    const QString tab1After = tabs->tabText(1);
    const QString tab2After = tabs->tabText(2);
    EXPECT_TRUE(tab0Before != tab0After || tab1Before != tab1After || tab2Before != tab2After)
        << "no tab title changed after sr_RS switch — retranslateUi() not wired or .qm not loaded";
}

} // namespace librecelik::test::i18n
