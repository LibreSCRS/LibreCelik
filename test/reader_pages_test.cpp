// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The reader selector and the page stack, and the two defects that lived in
// the gap between them while this logic sat inside the window — which no test
// binary could reach at the time.
//
// Both scenarios below are two-reader ones, because both defects need a
// bystander: a second card on screen while something happens to the first.

#include "utils/readerpages.h"

#include <gtest/gtest.h>

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QSignalSpy>
#include <QStackedWidget>

namespace {

// Selector row i, stack index i and position i of the card order must name the same card. Every
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
// inserting a second card moved the view off the reader the user was looking at.
TEST_F(ReaderPagesTest, ASecondCardDoesNotTakeTheViewFromTheFirst)
{
    Fixture f;
    f.addCard("card-A", "Reader A");

    f.addCard("card-B", "Reader B");

    EXPECT_EQ(f.pages.currentCardId(), QString("card-A")) << "inserting a second card moved the view";
    expectLockstep(f.pages, f.selector, f.stack);
}

// Removing a card whose selector row sits BELOW the current one renumbers the
// current row, and QComboBox emits currentIndexChanged while the stack still
// has the old layout. The handler must not act on that half-updated state: the
// card the user is looking at stays on screen.
TEST_F(ReaderPagesTest, RemovingACardKeepsAnotherReadersPageOnScreen)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");

    // The user is looking at B.
    f.selector.setCurrentIndex(1);
    ASSERT_EQ(f.pages.currentCardId(), QString("card-B"));

    f.pages.remove("card-A");

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

// Choosing a reader shows its page; the card left behind keeps its page (and,
// in the window, its read) untouched.
TEST_F(ReaderPagesTest, ChoosingAnotherReaderShowsItsPage)
{
    Fixture f;
    QWidget* pageA = f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Reader B");

    f.selector.setCurrentIndex(1);

    EXPECT_EQ(f.pages.currentCardId(), QString("card-B"));
    EXPECT_EQ(f.pages.page("card-A"), pageA) << "leaving a reader must not touch its page";
    expectLockstep(f.pages, f.selector, f.stack);
}

// A reader's name can arrive after its card's page: the label is renamed in
// place, without moving the view or touching any other row.
TEST_F(ReaderPagesTest, RelabellingARowKeepsTheViewAndTheOtherRows)
{
    Fixture f;
    f.addCard("card-A", "Reader A");
    f.addCard("card-B", "Card reader");
    f.selector.setCurrentIndex(1);
    QSignalSpy changed(&f.selector, &QComboBox::currentIndexChanged);

    f.pages.setLabel("card-B", "Reader B");
    f.pages.setLabel("card-unknown", "Reader X");

    EXPECT_EQ(f.selector.itemText(0), QString("Reader A"));
    EXPECT_EQ(f.selector.itemText(1), QString("Reader B"));
    EXPECT_EQ(f.pages.currentCardId(), QString("card-B"));
    EXPECT_EQ(changed.count(), 0) << "a rename announced a selection change";
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
