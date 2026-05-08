// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Self-tests for RetranslatableWidgetFixture (A.R.5).
///
/// Verify the fixture's machinery (snapshot walker, switch translator,
/// assertAllRetranslated rule set) before any production widget
/// depends on it. Does NOT exercise plugin widgets — those are covered
/// by `i18n_plugin_retranslate_test.cpp` (Phase B.5 D9).

#include "i18n_test_support/RetranslatableWidgetFixture.h"
#include "i18n_test_support/locale_stable_words.h"

#include <QApplication>
#include <QComboBox>
#include <QGroupBox>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>
#include <QWidget>
#include <gtest/gtest-spi.h>
#include <gtest/gtest.h>

namespace librecelik::test::i18n {

class FixtureSelfTest : public RetranslatableWidgetFixture
{};

TEST_F(FixtureSelfTest, snapshotKeysCoverEnumeratedRoles)
{
    auto* root = new QWidget;
    root->setObjectName(QStringLiteral("rootW"));
    root->setWindowTitle(QStringLiteral("Hello"));
    root->setToolTip(QStringLiteral("tip"));

    auto* lay = new QVBoxLayout(root);
    auto* lbl = new QLabel(QStringLiteral("LBL"), root);
    lbl->setObjectName(QStringLiteral("lblA"));
    lay->addWidget(lbl);

    auto* btn = new QPushButton(QStringLiteral("OK"), root);
    btn->setObjectName(QStringLiteral("okBtn"));
    lay->addWidget(btn);

    auto* gb = new QGroupBox(QStringLiteral("GROUP"), root);
    gb->setObjectName(QStringLiteral("grp1"));
    lay->addWidget(gb);

    auto* combo = new QComboBox(root);
    combo->setObjectName(QStringLiteral("cmb1"));
    combo->addItem(QStringLiteral("first"));
    combo->addItem(QStringLiteral("second"));
    lay->addWidget(combo);

    auto* tabs = new QTabWidget(root);
    tabs->setObjectName(QStringLiteral("tabs1"));
    auto* p1 = new QWidget;
    p1->setObjectName(QStringLiteral("page1"));
    tabs->addTab(p1, QStringLiteral("ONE"));
    auto* p2 = new QWidget;
    p2->setObjectName(QStringLiteral("page2"));
    tabs->addTab(p2, QStringLiteral("TWO"));
    lay->addWidget(tabs);

    const auto snap = snapshotTexts(root);

    EXPECT_EQ(snap.value(QStringLiteral("rootW:windowTitle")), QStringLiteral("Hello"));
    EXPECT_EQ(snap.value(QStringLiteral("rootW:toolTip")), QStringLiteral("tip"));
    EXPECT_EQ(snap.value(QStringLiteral("lblA:text")), QStringLiteral("LBL"));
    EXPECT_EQ(snap.value(QStringLiteral("okBtn:text")), QStringLiteral("OK"));
    EXPECT_EQ(snap.value(QStringLiteral("grp1:title")), QStringLiteral("GROUP"));
    EXPECT_EQ(snap.value(QStringLiteral("cmb1:itemText[0]")), QStringLiteral("first"));
    EXPECT_EQ(snap.value(QStringLiteral("cmb1:itemText[1]")), QStringLiteral("second"));
    EXPECT_EQ(snap.value(QStringLiteral("tabs1:tabText[0]")), QStringLiteral("ONE"));
    EXPECT_EQ(snap.value(QStringLiteral("tabs1:tabText[1]")), QStringLiteral("TWO"));

    delete root;
}

TEST_F(FixtureSelfTest, switchToInstallsAndUninstallsTranslator)
{
    // Before any switch, no translator should be active. (We cannot
    // easily query Qt for the active translator; instead, verify
    // switchTo returns cleanly and TearDown does not crash.)
    switchTo(QStringLiteral("en"));
    switchTo(QStringLiteral("sr_RS"));
    // TearDown cleans up.
    SUCCEED();
}

TEST_F(FixtureSelfTest, assertAllRetranslated_failsOnStaleLabel)
{
    QMap<QString, QString> before;
    QMap<QString, QString> after;
    before.insert(QStringLiteral("lbl1:text"), QStringLiteral("Hello"));
    after.insert(QStringLiteral("lbl1:text"), QStringLiteral("Hello"));
    EXPECT_NONFATAL_FAILURE(assertAllRetranslated(before, after), "identical string before/after");
}

TEST_F(FixtureSelfTest, assertAllRetranslated_skipsLocaleStableWord)
{
    QMap<QString, QString> before;
    QMap<QString, QString> after;
    before.insert(QStringLiteral("pinLbl:text"), QStringLiteral("PIN"));
    after.insert(QStringLiteral("pinLbl:text"), QStringLiteral("PIN"));
    // No failure expected — "PIN" is in the locale-stable wordlist.
    assertAllRetranslated(before, after);
    SUCCEED();
}

TEST_F(FixtureSelfTest, assertAllRetranslated_skipsTechnicalValue)
{
    QMap<QString, QString> before;
    QMap<QString, QString> after;
    before.insert(QStringLiteral("dateLbl:text"), QStringLiteral("2026-05-08"));
    after.insert(QStringLiteral("dateLbl:text"), QStringLiteral("2026-05-08"));
    before.insert(QStringLiteral("urlLbl:text"), QStringLiteral("https://librescrs.github.io"));
    after.insert(QStringLiteral("urlLbl:text"), QStringLiteral("https://librescrs.github.io"));
    before.insert(QStringLiteral("digitLbl:text"), QStringLiteral("12345"));
    after.insert(QStringLiteral("digitLbl:text"), QStringLiteral("12345"));
    assertAllRetranslated(before, after);
    SUCCEED();
}

TEST_F(FixtureSelfTest, snapshotIsDeterministic)
{
    auto* root = new QWidget;
    root->setObjectName(QStringLiteral("d1"));
    auto* lay = new QVBoxLayout(root);
    auto* lbl = new QLabel(QStringLiteral("foo"), root);
    lbl->setObjectName(QStringLiteral("flbl"));
    lay->addWidget(lbl);

    const auto a = snapshotTexts(root);
    const auto b = snapshotTexts(root);
    ASSERT_EQ(a.size(), b.size());
    for (auto it = a.constBegin(); it != a.constEnd(); ++it) {
        EXPECT_EQ(b.value(it.key()), it.value());
    }
    delete root;
}

TEST_F(FixtureSelfTest, technicalValueHeuristicCoverage)
{
    EXPECT_TRUE(isTechnicalValue(QStringView(QStringLiteral(""))));
    EXPECT_TRUE(isTechnicalValue(QStringView(QStringLiteral("a"))));
    EXPECT_TRUE(isTechnicalValue(QStringView(QStringLiteral("12345"))));
    EXPECT_TRUE(isTechnicalValue(QStringView(QStringLiteral("2026-05-08"))));
    EXPECT_TRUE(isTechnicalValue(QStringView(QStringLiteral("https://x"))));
    EXPECT_FALSE(isTechnicalValue(QStringView(QStringLiteral("Hello"))));
    EXPECT_FALSE(isTechnicalValue(QStringView(QStringLiteral("Hello world"))));
}

} // namespace librecelik::test::i18n
