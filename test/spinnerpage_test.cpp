// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// The read-in-progress page holds GUIDANCE, not a caption: the
// pre-authentication line tells the holder where on the document to find the
// CAN or MRZ the agent is about to ask for. Guidance that renders with its
// last lines cut off is worse than none — the holder cannot tell there was
// more to read. So the cases below give the page text long enough to wrap at
// the width they lay it out at, and assert the label is given every line it
// needs.

#include "utils/spinnerpage.h"

#include <QApplication>
#include <QLabel>
#include <QLayout>
#include <QString>
#include <QStringLiteral>
#include <QWidget>

#include <gtest/gtest.h>

using librecelik::utils::isSpinner;
using librecelik::utils::makeSpinnerPage;

namespace {

/// The guided pre-authentication text's shape: several sentences, no line the
/// layout could fit whole at a dialog width.
QString longGuidedText()
{
    return QStringLiteral(
        "Unesite CAN sa dokumenta u prozoru agenta. CAN je šestocifreni broj odštampan na prednjoj strani "
        "dokumenta, u donjem desnom uglu, ispod fotografije. Ako dokument nema CAN, agent će zatražiti MRZ "
        "sa poleđine dokumenta. Nijedna od ovih vrednosti se ne unosi u ovom prozoru.");
}

/// The page, shown offscreen at @p width and laid out, plus its label.
struct LaidOutPage
{
    QWidget* page = nullptr;
    QLabel* label = nullptr;
};

LaidOutPage layOut(QWidget* page, int width, int height)
{
    page->resize(width, height);
    page->show();
    page->layout()->activate();
    QApplication::processEvents();
    return LaidOutPage{page, page->findChild<QLabel*>()};
}

} // namespace

class SpinnerPageTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
    }
    static QApplication* app;
};
QApplication* SpinnerPageTest::app = nullptr;

TEST_F(SpinnerPageTest, PageIsRecognisableAsTheSpinner)
{
    QWidget owner;
    EXPECT_TRUE(isSpinner(makeSpinnerPage(QStringLiteral("reading"), &owner)));
    EXPECT_FALSE(isSpinner(&owner));
    EXPECT_FALSE(isSpinner(nullptr));
}

/// Assert the guided text is laid out whole on a @p pageWidth-wide page: the
/// label wraps at the page's width rather than at some narrower slot, and it
/// is tall enough for every line that width costs it.
void expectNothingClipped(int pageWidth)
{
    QWidget owner;
    const LaidOutPage laid = layOut(makeSpinnerPage(longGuidedText(), &owner), pageWidth, 600);
    ASSERT_NE(laid.label, nullptr);
    ASSERT_TRUE(laid.label->wordWrap());

    // The label wraps at the PAGE's width. It used to be capped at the
    // progress bar's fixed width, which the column inherited.
    EXPECT_GT(laid.label->width(), 200) << "label capped below the page width";

    const int needed = laid.label->heightForWidth(laid.label->width());
    // Non-vacuous: the text really does wrap here, so a page that grants a
    // single line's height cannot pass by accident.
    ASSERT_GT(needed, laid.label->fontMetrics().height()) << "text did not wrap — widen the fixture text";
    EXPECT_GE(laid.label->height(), needed) << "label is " << laid.label->height() << " px tall but needs " << needed
                                            << " px at width " << laid.label->width();
}

TEST_F(SpinnerPageTest, WrappedGuidanceGetsEveryLineItNeeds)
{
    expectNothingClipped(420); // a plausible reader-panel width
}

TEST_F(SpinnerPageTest, NarrowPageStillShowsEveryLine)
{
    // Narrower: more lines, same rule. A layout that sizes the label from a
    // hint computed at some other width fails one of these two widths.
    expectNothingClipped(300);
}

TEST_F(SpinnerPageTest, ShortTextStaysCentredAndUnstretched)
{
    // The page is a placeholder, not a panel: a one-line wait message must not
    // grow into a tall block just because the fix gave the layout the whole
    // page. The label keeps its own height and the progress bar keeps its
    // fixed width.
    QWidget owner;
    const LaidOutPage laid = layOut(makeSpinnerPage(QStringLiteral("Čitanje kartice…"), &owner), 420, 600);
    ASSERT_NE(laid.label, nullptr);
    EXPECT_LE(laid.label->height(), laid.label->sizeHint().height() + 2);
}
