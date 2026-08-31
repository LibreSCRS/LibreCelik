// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include <QLabel>
#include <QList>
#include <QMouseEvent>
#include <QPointF>
#include "utils/collapsiblesection.h"
#include "utils/securitystatuswidget.h"

class SecurityStatusWidgetTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!QApplication::instance()) {
            int argc = 0;
            app = new QApplication(argc, nullptr);
        }
    }

    // The reader's choice for the per-check block is process-wide by design —
    // it has to outlive the pane that heard it. That makes it a thing one test
    // can hand to the next, so every case starts and ends without one.
    void SetUp() override
    {
        librecelik::utils::forgetDetailChecksChoice();
    }
    void TearDown() override
    {
        librecelik::utils::forgetDetailChecksChoice();
    }

    static QApplication* app;
};
QApplication* SecurityStatusWidgetTest::app = nullptr;

using librecelik::utils::SecurityCategory;
using librecelik::utils::SecurityCheck;
using librecelik::utils::SecurityStatusModel;

TEST_F(SecurityStatusWidgetTest, RendersWithoutCrash)
{
    SecurityStatusWidget widget;
    SecurityStatusModel status;
    status.overallIntegrity = SecurityCheck::Status::Passed;
    status.overallAuthenticity = SecurityCheck::Status::Passed;
    status.overallGenuineness = SecurityCheck::Status::NotPerformed;
    widget.setSecurityStatus(status);
    // Widget renders without crash
}

TEST_F(SecurityStatusWidgetTest, RendersWithDetailChecks)
{
    SecurityStatusWidget widget;
    SecurityStatusModel status;
    status.overallIntegrity = SecurityCheck::Status::Passed;
    status.overallAuthenticity = SecurityCheck::Status::Failed;
    status.overallGenuineness = SecurityCheck::Status::NotSupported;

    SecurityCheck check;
    check.checkId = "hash_dg1";
    check.category = SecurityCategory::DataIntegrity;
    check.status = SecurityCheck::Status::Passed;
    check.label = "DG1 Hash";
    check.detail = "Hash matches SOD";
    status.checks.push_back(check);

    check.checkId = "ds_cert";
    check.category = SecurityCategory::Authenticity;
    check.status = SecurityCheck::Status::Failed;
    check.label = "DS Certificate";
    check.detail = "Certificate expired";
    status.checks.push_back(check);

    widget.setSecurityStatus(status);
    // Widget renders without crash with detail checks
}

TEST_F(SecurityStatusWidgetTest, UpdateStatusTwice)
{
    SecurityStatusWidget widget;

    SecurityStatusModel status1;
    status1.overallIntegrity = SecurityCheck::Status::NotPerformed;
    widget.setSecurityStatus(status1);

    SecurityStatusModel status2;
    status2.overallIntegrity = SecurityCheck::Status::Passed;
    status2.overallAuthenticity = SecurityCheck::Status::Passed;
    status2.overallGenuineness = SecurityCheck::Status::Passed;
    widget.setSecurityStatus(status2);
    // Widget handles status update without crash
}

// --- the reason a signer verdict carries ------------------------------------
//
// The reader that judges a travel document's signer against this
// installation's trust anchors reports WHY as a stable key, the same token in
// every language. Turning it into a sentence is this host's job, and the
// resolution rule is the one the field-label grid already uses: a key this
// build names renders its own catalogue string, and a key it does not name
// falls back to something rather than to nothing.
//
// These cases run WITHOUT a translator installed, so qtTrId() hands back the
// bare id. That is exactly enough to tell the two arms apart: a named key
// answers with its catalogue id, an unnamed one answers with the key.

TEST_F(SecurityStatusWidgetTest, EveryNamedReasonKeyResolvesToACatalogueString)
{
    for (const QString& key : {QStringLiteral("csca.not-configured"), QStringLiteral("csca.anchors-unreadable"),
                               QStringLiteral("csca.anchors-undecodable"), QStringLiteral("csca.no-anchor-for-issuer"),
                               QStringLiteral("csca.chain-failed")}) {
        const QString text = librecelik::utils::localizedReasonText(key);
        EXPECT_FALSE(text.isEmpty()) << "no text for " << qPrintable(key);
        EXPECT_NE(text, key) << "reason " << qPrintable(key) << " has no arm and fell through to the raw key";
    }
}

// The case a catalogue change is most likely to break later: a reader newer
// than this build names a reason nobody here has heard of. Erasing it would
// cost the holder the only record that the signer was not checked; printing
// "unknown" would replace a token a support report can act on with a word that
// says nothing.
TEST_F(SecurityStatusWidgetTest, UnnamedReasonKeyFallsBackToTheKeyItself)
{
    const QString future = QStringLiteral("csca.a-reason-from-a-later-build");
    EXPECT_EQ(librecelik::utils::localizedReasonText(future), future);

    // Not even a csca.* key -- a whole new family must degrade the same way.
    const QString other = QStringLiteral("dsc.revoked");
    EXPECT_EQ(librecelik::utils::localizedReasonText(other), other);
}

// No reason is the ordinary case: every check that simply passed. It must stay
// empty so the pane draws no blank line under it.
TEST_F(SecurityStatusWidgetTest, AbsentReasonStaysAbsent)
{
    EXPECT_TRUE(librecelik::utils::localizedReasonText(QString()).isEmpty());
}

// --- the per-check block is a section, and its default says what to expect ---
//
// The three roll-up verdicts are three words. The per-check block under them is
// eight rows with a wrapped paragraph under any that did not pass, and it
// pushed the holder's own data off the bottom of the pane. Every other block at
// that level already collapses, so this one does too — but a plain "closed by
// default" would undo the work the block exists for, because the paragraph
// under a check that did not run is the only line telling the reader what they
// can DO about it.
//
// So the default is DERIVED: nothing to act on closes it, something to act on
// opens it, and an open block is itself the signal that this read wants
// attention.

namespace {

/// The per-check block, found the way a reader finds it: by the heading it
/// carries. Null while the block is not a section at all.
CollapsibleSection* detailBlockOf(QWidget& widget)
{
    for (CollapsibleSection* section : widget.findChildren<CollapsibleSection*>()) {
        if (section->title() == qtTrId("lc-emrtd-security-details")) {
            return section;
        }
    }
    return nullptr;
}

SecurityCheck checkWith(SecurityCheck::Status status, const QString& label, const QString& reason = QString())
{
    SecurityCheck check;
    check.checkId = label.toLower();
    check.category = SecurityCategory::Authenticity;
    check.status = status;
    check.label = label;
    check.reason = reason;
    return check;
}

/// A read whose checks all succeeded — the ordinary passport on a machine whose
/// trust anchors are in place.
SecurityStatusModel everyCheckPassed()
{
    SecurityStatusModel status;
    status.overallIntegrity = SecurityCheck::Status::Passed;
    status.overallAuthenticity = SecurityCheck::Status::Passed;
    status.overallGenuineness = SecurityCheck::Status::Passed;
    status.checks.push_back(checkWith(SecurityCheck::Status::Passed, QStringLiteral("DG1 Hash")));
    status.checks.push_back(checkWith(SecurityCheck::Status::Passed, QStringLiteral("Passive Authentication")));
    status.checks.push_back(checkWith(SecurityCheck::Status::Passed, QStringLiteral("Chip Authentication")));
    return status;
}

/// The same read on a machine with no country signing certificates: the signer
/// check did not run, and its reason is the only actionable line on the pane.
SecurityStatusModel signerCheckNeverRan()
{
    SecurityStatusModel status = everyCheckPassed();
    status.overallAuthenticity = SecurityCheck::Status::NotPerformed;
    status.checks[1] = checkWith(SecurityCheck::Status::NotPerformed, QStringLiteral("Passive Authentication"),
                                 QStringLiteral("csca.not-configured"));
    return status;
}

} // namespace

TEST_F(SecurityStatusWidgetTest, EveryCheckPassedLeavesTheDetailBlockClosed)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(everyCheckPassed());

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr) << "the per-check block is not a section a reader can close";
    EXPECT_FALSE(block->isExpanded()) << "nothing to act on, yet the block still buries the holder's data";
}

// The block never opens itself, whatever the verdict. It used to open on
// Failed or NotPerformed, and on a real document that meant ALWAYS: DG3 holds
// fingerprints, which an ordinary read never has the authorization to fetch,
// so every passport arrived with a NotPerformed row and the block stood open
// on all of them. The rule already excluded NOT_SUPPORTED and SKIPPED for
// exactly that reason -- "a block left open on ordinary documents stops
// meaning anything" -- and NotPerformed was the case that reason described
// best. Rather than trim the trigger list again, the automatic trigger is
// gone: the three summary verdicts above the block carry the outcome, and a
// reader who wants the per-check breakdown opens it.
TEST_F(SecurityStatusWidgetTest, ACheckNobodyRanStillLeavesTheDetailBlockClosed)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(signerCheckNeverRan());

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr) << "the per-check block is not a section a reader can close";
    EXPECT_FALSE(block->isExpanded()) << "an unrun check popped the block open; on a passport DG3 is always unrun, "
                                         "so this is every document, every read";
}

TEST_F(SecurityStatusWidgetTest, AFailedCheckStillLeavesTheDetailBlockClosed)
{
    SecurityStatusModel status = everyCheckPassed();
    status.overallIntegrity = SecurityCheck::Status::Failed;
    status.checks[0] = checkWith(SecurityCheck::Status::Failed, QStringLiteral("DG1 Hash"));

    SecurityStatusWidget widget;
    widget.setSecurityStatus(status);

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr);
    // The failure is NOT hidden by this: overallIntegrity above the block says
    // so in its own row, which TheThreeSummaryVerdictsStayOutsideTheBlock pins.
    EXPECT_FALSE(block->isExpanded()) << "the block opened itself on a failure; the summary row already carries that "
                                         "verdict, and a block that opens on its own is the behaviour being removed";
}

// Closed, the block still has to say what is inside it — otherwise the reader
// is asked to click something unnamed to find out whether it matters.
TEST_F(SecurityStatusWidgetTest, TheClosedDetailBlockStillNamesItself)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(everyCheckPassed());

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr);
    ASSERT_FALSE(block->isExpanded());
    EXPECT_EQ(block->title(), qtTrId("lc-emrtd-security-details"));
}

// A closed block has to be EMPTY on screen, not merely short. The rows are
// rebuilt on every arriving verdict, and a row added to the layout of a section
// the reader has closed is a new child the layout will happily show — drawn
// straight over the heading it was supposed to be behind.
TEST_F(SecurityStatusWidgetTest, RowsRebuiltUnderAClosedHeadingStayHidden)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(signerCheckNeverRan()); // opens
    widget.setSecurityStatus(everyCheckPassed());    // ...and closes again, rebuilding the rows

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr);
    ASSERT_FALSE(block->isExpanded());

    const QList<QLabel*> rows = block->findChildren<QLabel*>(QStringLiteral("checkLabel"));
    ASSERT_FALSE(rows.isEmpty()) << "no per-check rows were built at all";
    for (const QLabel* row : rows) {
        EXPECT_TRUE(row->isHidden()) << "row \"" << qPrintable(row->text()) << "\" is drawn under a closed heading";
    }
}

// The three roll-up verdicts are the answer the reader came for. They stay
// outside the block that collapses, or closing it would hide the verdict too.
TEST_F(SecurityStatusWidgetTest, TheThreeSummaryVerdictsStayOutsideTheBlock)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(everyCheckPassed());

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr);

    const QList<QLabel*> summary = widget.findChildren<QLabel*>(QStringLiteral("text"));
    ASSERT_EQ(summary.size(), 3) << "the pane no longer carries exactly the three roll-up rows";
    for (const QLabel* label : summary) {
        for (const QObject* parent = label->parent(); parent != nullptr; parent = parent->parent()) {
            EXPECT_NE(parent, block) << "summary verdict \"" << qPrintable(label->text())
                                     << "\" was moved inside the collapsing block";
        }
    }
}

// --- the reader's own choice outranks the derived default -------------------

namespace {

/// Toggle the section the way a reader does: a press inside the header bar.
///
/// Deliberately the input path rather than setExpanded(). The whole point of
/// the memory is to tell a person's choice from the program's, and a test that
/// set the state directly would prove nothing about that distinction.
void clickHeaderOf(CollapsibleSection& section)
{
    const QPointF inHeader(40.0, 8.0);
    QMouseEvent press(QEvent::MouseButtonPress, inHeader, section.mapToGlobal(inHeader), Qt::LeftButton, Qt::LeftButton,
                      Qt::NoModifier);
    QApplication::sendEvent(&section, &press);
}

} // namespace

// A reader who opens the block and then closes it again is back to closed, and
// stays there on the next card. The remembered choice has to be UPDATED by the
// second press, not merely set by the first — a memory that only ever records
// "opened" would pass the sibling test below while quietly ignoring this one.
//
// The block must be opened by hand first: nothing opens it on its own any more,
// so a close that starts from closed would prove nothing, which is what the
// assertion this test used to carry was guarding against.
TEST_F(SecurityStatusWidgetTest, AReaderWhoClosesTheBlockKeepsItClosedOnTheNextRead)
{
    {
        SecurityStatusWidget firstRead;
        firstRead.setSecurityStatus(signerCheckNeverRan());
        CollapsibleSection* block = detailBlockOf(firstRead);
        ASSERT_NE(block, nullptr);
        ASSERT_FALSE(block->isExpanded()) << "the block opened itself; this test can no longer measure a close";
        clickHeaderOf(*block);
        ASSERT_TRUE(block->isExpanded()) << "the header press did not open the block";
        clickHeaderOf(*block);
        ASSERT_FALSE(block->isExpanded()) << "the second header press did not close the block again";
    }

    ASSERT_TRUE(librecelik::utils::rememberedDetailChecksChoice().has_value())
        << "two presses by a person left no recorded choice at all";
    EXPECT_FALSE(*librecelik::utils::rememberedDetailChecksChoice())
        << "the close was not recorded over the open: the memory keeps the FIRST press, so a reader can open the "
           "block once and never be able to put it back";

    // The next card: a whole new pane, because every read builds one.
    SecurityStatusWidget nextRead;
    nextRead.setSecurityStatus(signerCheckNeverRan());
    CollapsibleSection* block = detailBlockOf(nextRead);
    ASSERT_NE(block, nullptr);
    EXPECT_FALSE(block->isExpanded()) << "the block re-opened itself over a section the reader had closed";
}

// And the same in the other direction: a reader who opens a quiet read's block
// keeps it open, rather than having it shut on them at the next card.
TEST_F(SecurityStatusWidgetTest, AReaderWhoOpensTheBlockKeepsItOpenOnTheNextRead)
{
    {
        SecurityStatusWidget firstRead;
        firstRead.setSecurityStatus(everyCheckPassed());
        CollapsibleSection* block = detailBlockOf(firstRead);
        ASSERT_NE(block, nullptr);
        ASSERT_FALSE(block->isExpanded());
        clickHeaderOf(*block);
        ASSERT_TRUE(block->isExpanded()) << "the header press did not open the block";
    }

    SecurityStatusWidget nextRead;
    nextRead.setSecurityStatus(everyCheckPassed());
    CollapsibleSection* block = detailBlockOf(nextRead);
    ASSERT_NE(block, nullptr);
    EXPECT_TRUE(block->isExpanded()) << "the block closed itself over a section the reader had opened";
}

// The memory has to be of a PERSON's choice. A section this code opened or
// closed on the reader's behalf must not be recorded as something they asked
// for, or the very first derived default would freeze and no read after it
// would ever be judged on its own outcome.
TEST_F(SecurityStatusWidgetTest, TheProgramsOwnToggleIsNotMistakenForTheReadersChoice)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(everyCheckPassed());
    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr);

    block->setExpanded(true);
    EXPECT_FALSE(librecelik::utils::rememberedDetailChecksChoice().has_value())
        << "a programmatic toggle was filed as the reader's own decision";
}

// The rows are rebuilt whenever a verdict re-arrives or the language changes.
// Each check row is a NESTED layout, and deleting a layout does not delete the
// widgets it arranged — so the rows used to stay behind as orphaned children of
// the block, one full set per rebuild, drawn at whatever geometry they last
// had. Making the block collapsible put those orphans inside something a reader
// opens, which is where they would finally be seen.
TEST_F(SecurityStatusWidgetTest, RebuildingTheRowsReplacesThemRatherThanStackingThem)
{
    SecurityStatusWidget widget;
    widget.setSecurityStatus(everyCheckPassed());

    CollapsibleSection* block = detailBlockOf(widget);
    ASSERT_NE(block, nullptr);
    const qsizetype first = block->findChildren<QLabel*>(QStringLiteral("checkLabel")).size();
    ASSERT_EQ(first, 3);

    widget.setSecurityStatus(everyCheckPassed());
    EXPECT_EQ(block->findChildren<QLabel*>(QStringLiteral("checkLabel")).size(), first)
        << "the rows from the previous build stayed behind";

    widget.setSecurityStatus(signerCheckNeverRan());
    EXPECT_EQ(block->findChildren<QLabel*>(QStringLiteral("checkLabel")).size(), first)
        << "the rows from the previous build stayed behind";
}
