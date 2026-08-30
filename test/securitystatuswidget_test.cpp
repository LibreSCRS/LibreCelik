// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
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
