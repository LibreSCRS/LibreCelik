// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief D9 runtime test — verify that every plugin card widget
///        retranslates dynamic content when the application language
///        switches at runtime.
///
/// Each plugin widget's retranslateUi() is supposed to tear down its
/// dynamic sections (built progressively from the agent's field groups via
/// FieldSectionBuilder + qtTrId-derived translation maps) and rebuild
/// from cached state in the new language. This test parameterises over
/// all 5 production plugin widgets, builds each one with a synthetic
/// field-group read, snapshots translatable widget state, switches
/// translator, snapshots again, and asserts every shared key changed.
///
/// The recursive widget walk keys each snapshotted role by DFS position,
/// which drifts across the two snapshots for anything a rebuild tears down
/// and reconstructs: `deleteLater()` does not destroy the previous
/// generation synchronously, so a stale section is still a child when the
/// second snapshot is taken, and the position the walk assigns to the live
/// section no longer lines up with where it was in the first snapshot. The
/// outer section — the plugin widget's own heading, present in every
/// plugin — is read directly instead (outerSectionOf()) and folded into
/// both snapshots under a fixed key, sidestepping the drift for that one
/// role.
///
/// All tests run with QT_QPA_PLATFORM=offscreen.

#include "i18n_test_support/RetranslatableWidgetFixture.h"
#include "i18n_test_support/mock_plugin_data.h"

#include "emrtd/emrtdwidget.h"
#include "eu-vrc/euvrcwidget.h"
#include "piv/pivwidget.h"
#include "rs-eid/eidwidget.h"
#include "rs-health/healthwidget.h"
#include "utils/collapsiblesection.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QWidget>
#include <gtest/gtest.h>

#include <algorithm>
#include <functional>
#include <memory>
#include <string>

namespace librecelik::test::i18n {

namespace {

/// @brief The plugin widget's own outer collapsible section — a direct
/// child of the widget under test in every one of the 5 plugins. A
/// retranslateUi() rebuild constructs the replacement before the old
/// section is actually destroyed (deleteLater() only schedules that), so
/// more than one generation can be a direct child at once; Qt appends a
/// new child at the end of the parent's child list, so the last one is
/// always the current, on-screen generation.
CollapsibleSection* outerSectionOf(QWidget* root)
{
    const auto sections = root->findChildren<CollapsibleSection*>(QString(), Qt::FindDirectChildrenOnly);
    return sections.isEmpty() ? nullptr : sections.constLast();
}

} // namespace

/// @brief Plugin-widget factory: build a fresh QWidget* of the given
/// plugin type, populated with the corresponding mock field groups. The
/// test asserts retranslate behaviour on the returned widget.
struct PluginCase
{
    std::string name;
    std::function<QWidget*()> build;
};

class PluginRetranslateTest : public RetranslatableWidgetFixture, public ::testing::WithParamInterface<PluginCase>
{};

TEST_P(PluginRetranslateTest, dynamicLabelsRetranslateOnLanguageChange)
{
    const auto& tc = GetParam();
    SCOPED_TRACE(tc.name);

    std::unique_ptr<QWidget> widget(tc.build());
    ASSERT_NE(widget.get(), nullptr);
    widget->show();
    QCoreApplication::processEvents();

    // Snapshot in the default (English / qtTrId source IDs) state.
    switchTo(QStringLiteral("en"));
    QCoreApplication::processEvents();
    auto before = snapshotTexts(widget.get());
    ASSERT_FALSE(before.isEmpty()) << "snapshot empty — widget did not populate any translatable role";
    // The walk above does not reliably see the outer heading (see the
    // file comment) — read it directly under a DFS-independent key.
    if (auto* outer = outerSectionOf(widget.get()))
        before.insert(QStringLiteral("outerSection:heading"), outer->title());

    // Switch language and confirm Qt sent QEvent::LanguageChange (the
    // PluginWidgetBase base class observes this and invokes
    // retranslateUi(); each plugin's override rebuilds dynamic content).
    switchTo(QStringLiteral("sr_RS"));
    QCoreApplication::processEvents();

    auto after = snapshotTexts(widget.get());
    ASSERT_FALSE(after.isEmpty());
    if (auto* outer = outerSectionOf(widget.get()))
        after.insert(QStringLiteral("outerSection:heading"), outer->title());

    // Tolerate equal strings only when locale-stable wordlist or
    // technical-value heuristic apply (numbers, dates, URLs, the
    // wordlist of acronyms like "PIN" / "PIV" / "OK"). Anything else
    // identical between snapshots is a real retranslate bug.
    assertAllRetranslated(before, after);
}

namespace {

QWidget* buildEid()
{
    return new EidWidget(mock::makeEidMock());
}

QWidget* buildHealth()
{
    return new HealthWidget(mock::makeHealthMock());
}

QWidget* buildEmrtd()
{
    return new EMRTDWidget(mock::makeEmrtdMock());
}

QWidget* buildEuVrc()
{
    return new EuVrcWidget(mock::makeEuVrcMock());
}

QWidget* buildPiv()
{
    return new PIVWidget(mock::makePivMock());
}

} // namespace

INSTANTIATE_TEST_SUITE_P(AllPlugins, PluginRetranslateTest,
                         ::testing::Values(PluginCase{"EidWidget", &buildEid}, PluginCase{"HealthWidget", &buildHealth},
                                           PluginCase{"EMRTDWidget", &buildEmrtd},
                                           PluginCase{"EuVrcWidget", &buildEuVrc}, PluginCase{"PIVWidget", &buildPiv}),
                         [](const ::testing::TestParamInfo<PluginCase>& info) { return info.param.name; });

/// Copy the widget walk above cannot reach, asserted against the catalog
/// directly.
///
/// The mock PIV read carries a discovery field group, so its section is
/// built and its outer heading is folded into the snapshot by
/// outerSectionOf() — but "lc-piv-section-discovery" itself names a nested
/// section (a child of the outer section, not of the widget), which is
/// exactly the DFS-drift case the file comment describes: it is not read
/// directly, and its walk-assigned key is not guaranteed to line up between
/// the two snapshots. It still has a live call site in the plugin, so a
/// Serbian entry that is still the English source is a real gap this suite
/// would otherwise miss.
class PluginCatalogTest : public RetranslatableWidgetFixture
{
protected:
    /// The rendering of @p id under @p language, with the fixture's translator
    /// swapped for the duration of the call.
    [[nodiscard]] QString rendered(const QString& language, const char* id)
    {
        switchTo(language);
        QCoreApplication::processEvents();
        return qtTrId(id);
    }

    [[nodiscard]] static bool hasCyrillic(const QString& text)
    {
        return std::ranges::any_of(text, [](QChar c) { return c.script() == QChar::Script_Cyrillic; });
    }
};

TEST_F(PluginCatalogTest, PivCopyOutsideTheWidgetWalkIsTranslated)
{
    for (const char* id : {"lc-piv-widget-title", "lc-piv-section-discovery"}) {
        SCOPED_TRACE(id);
        const QString english = rendered(QStringLiteral("en"), id);
        const QString serbian = rendered(QStringLiteral("sr_RS"), id);

        ASSERT_FALSE(english.isEmpty()) << "id absent from the English catalog";
        ASSERT_FALSE(serbian.isEmpty()) << "id absent from the Serbian catalog";
        EXPECT_NE(serbian, english) << "the Serbian entry is still the English source";
        // Serbian ships in Cyrillic, so script is what separates a translation
        // from an untouched copy that happens to differ by punctuation.
        EXPECT_TRUE(hasCyrillic(serbian)) << "rendered \"" << serbian.toStdString() << "\", which is not Serbian";
    }
}

} // namespace librecelik::test::i18n
