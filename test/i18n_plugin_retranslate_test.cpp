// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief D9 runtime test — verify that every plugin card widget
///        retranslates dynamic content when the application language
///        switches at runtime.
///
/// Each plugin widget's retranslateUi() is supposed to tear down its
/// dynamic sections (built progressively from CardFieldGroups via
/// FieldSectionBuilder + qtTrId-derived translation maps) and rebuild
/// from cached state in the new language. This test parameterises over
/// all 5 production plugin widgets, builds each one with a synthetic
/// CardData, snapshots translatable widget state, switches translator,
/// snapshots again, and asserts every shared key changed.
///
/// All tests run with QT_QPA_PLATFORM=offscreen.

#include "i18n_test_support/RetranslatableWidgetFixture.h"
#include "i18n_test_support/mock_plugin_data.h"

#include "plugins/emrtd/emrtdwidget.h"
#include "plugins/eu-vrc/euvrcwidget.h"
#include "plugins/piv/pivwidget.h"
#include "plugins/rs-eid/eidwidget.h"
#include "plugins/rs-health/healthwidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QWidget>
#include <gtest/gtest.h>

#include <functional>
#include <memory>
#include <string>

namespace librecelik::test::i18n {

/// @brief Plugin-widget factory: build a fresh QWidget* of the given
/// plugin type, populated with the corresponding mock CardData. The
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
    const auto before = snapshotTexts(widget.get());
    ASSERT_FALSE(before.isEmpty()) << "snapshot empty — widget did not populate any translatable role";

    // Switch language and confirm Qt sent QEvent::LanguageChange (the
    // PluginWidgetBase base class observes this and invokes
    // retranslateUi(); each plugin's override rebuilds dynamic content).
    switchTo(QStringLiteral("sr_RS"));
    QCoreApplication::processEvents();

    const auto after = snapshotTexts(widget.get());
    ASSERT_FALSE(after.isEmpty());

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

} // namespace librecelik::test::i18n
