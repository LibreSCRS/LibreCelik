// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The reader selector and the page stack, and the two defects that lived in
// the gap between them while this logic sat inside the window — where no test
// binary could reach it (test/CMakeLists.txt excludes librecelik.cpp
// deliberately, and agent_mainflow_test.cpp says so at the top).
//
// Both scenarios below are two-reader ones, because both defects need a
// bystander: a second card whose read is in flight while something happens to
// the first.

#include "utils/readerpages.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QStackedWidget>

namespace {

// Selector row i, stack index i and cardIds()[i] must name the same card. Every
// assertion below leans on this, so check it as a unit.
void expectLockstep(const ReaderPages& pages, const QComboBox& selector, const QStackedWidget& stack)
{
    ASSERT_EQ(selector.count(), pages.count()) << "selector row count drifted from the card count";
    ASSERT_EQ(stack.count(), pages.count()) << "stack page count drifted from the card count";
    EXPECT_EQ(selector.currentIndex(), stack.currentIndex())
        << "the selector names one reader while the window shows another";
}

struct Fixture
{
    QComboBox selector;
    QStackedWidget stack;
    ReaderPages pages{&selector, &stack};

    QWidget* addCard(const QString& cardId, const QString& reader)
    {
        auto* page = new QLabel(cardId);
        pages.add(cardId, reader, page);
        return page;
    }
};

} // namespace

// QWidget construction needs a QApplication; gtest_main does not create one.
class ReaderPagesTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (QApplication::instance() == nullptr) {
            static int argc = 0;
            static char* argv[] = {nullptr};
            app = new QApplication(argc, argv);
        }
    }
    static QApplication* app;
};
QApplication* ReaderPagesTest::app = nullptr;

// The defect: registering a page called setCurrentIndex() unconditionally, so
// inserting a second card moved the view off the first — and leaving a page
// cancels its read, so the first reader's in-flight read died.
TEST_F(ReaderPagesTest, ASecondCardDoesNotTakeTheViewFromTheFirst)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    QSignalSpy left(&f.pages, &ReaderPages::leftCard);

    f.addCard("card-B", "Reader B");

    EXPECT_EQ(f.pages.currentCardId(), QString("card-A")) << "inserting a second card moved the view";
    EXPECT_EQ(left.count(), 0) << "inserting a card cancelled another reader's read";
    expectLockstep(f.pages, f.selector, f.stack);
}

// The defect this file exists for. Removing a card whose selector row sits
// BELOW the current one renumbers the current row, QComboBox emits
// currentIndexChanged, and the old handler read the stack — not yet updated —
// concluded the user had navigated away from the OTHER card, and cancelled that
// card's read. The user pulls card A and card B's read dies.
TEST_F(ReaderPagesTest, RemovingACardDoesNotCancelAnotherReadersRead)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");

    // The user is looking at B, whose read is in flight.
    f.selector.setCurrentIndex(1);
    ASSERT_EQ(f.pages.currentCardId(), QString("card-B"));

    QSignalSpy left(&f.pages, &ReaderPages::leftCard);
    f.pages.remove("card-A");

    for (int i = 0; i < left.count(); ++i) {
        EXPECT_NE(left.at(i).at(0).toString(), QString("card-B"))
            << "removing card A cancelled card B's in-flight read";
    }
    EXPECT_EQ(f.pages.currentCardId(), QString("card-B")) << "the surviving card's page must stay on screen";
    expectLockstep(f.pages, f.selector, f.stack);
}

// The defect: replacing a page switched the stack unconditionally while the
// selector stayed put. Every read replaces its page on the first streamed
// group, so a background reader dragged the view off the reader the user had
// chosen, and the selector then named a reader that was not on screen.
TEST_F(ReaderPagesTest, ABackgroundReaderReplacingItsPageDoesNotStealTheView)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");
    ASSERT_EQ(f.pages.currentCardId(), QString("card-A"));

    f.pages.replace("card-B", new QLabel("card-B read"));

    EXPECT_EQ(f.pages.currentCardId(), QString("card-A")) << "a background reader's page replacement stole the view";
    expectLockstep(f.pages, f.selector, f.stack);
}

// The other half of replace(): when the visible page is the one being swapped,
// the view must follow it, or a finished read would leave the spinner up.
TEST_F(ReaderPagesTest, ReplacingTheVisiblePageKeepsItVisible)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");
    auto* replacement = new QLabel("card-A read");

    f.pages.replace("card-A", replacement);

    EXPECT_EQ(f.pages.currentCardId(), QString("card-A"));
    EXPECT_EQ(f.stack.currentWidget(), replacement) << "the finished read's page must be the one on screen";
    expectLockstep(f.pages, f.selector, f.stack);
}

// Navigating away deliberately MUST still cancel — that is the behaviour the
// removal path was accidentally borrowing, and it has to survive the fix.
TEST_F(ReaderPagesTest, ChoosingAnotherReaderCancelsTheOneLeftBehind)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");
    QSignalSpy left(&f.pages, &ReaderPages::leftCard);

    f.selector.setCurrentIndex(1);

    ASSERT_EQ(left.count(), 1) << "leaving a reader must stop its read";
    EXPECT_EQ(left.at(0).at(0).toString(), QString("card-A"));
    EXPECT_EQ(f.pages.currentCardId(), QString("card-B"));
    expectLockstep(f.pages, f.selector, f.stack);
}

// Removing the card being looked at has to leave something coherent on screen.
TEST_F(ReaderPagesTest, RemovingTheVisibleCardFallsBackToASurvivor)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");
    ASSERT_EQ(f.pages.currentCardId(), QString("card-A"));

    f.pages.remove("card-A");

    EXPECT_EQ(f.pages.count(), 1);
    EXPECT_EQ(f.pages.currentCardId(), QString("card-B"));
    expectLockstep(f.pages, f.selector, f.stack);
}

TEST_F(ReaderPagesTest, RemovingTheLastCardLeavesNothingSelected)
{
    Fixture f;
    f.addCard("card-A", "Reader A");

    f.pages.remove("card-A");

    EXPECT_TRUE(f.pages.isEmpty());
    EXPECT_EQ(f.pages.currentCardId(), QString());
    EXPECT_EQ(f.selector.currentIndex(), -1);
}
