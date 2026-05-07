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

TEST_F(SecurityStatusWidgetTest, RendersWithoutCrash)
{
    SecurityStatusWidget widget;
    LibreSCRS::Plugin::SecurityStatus status;
    status.overallIntegrity = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    status.overallAuthenticity = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    status.overallGenuineness = LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed;
    widget.setSecurityStatus(status);
    // Widget renders without crash
}

TEST_F(SecurityStatusWidgetTest, RendersWithDetailChecks)
{
    SecurityStatusWidget widget;
    LibreSCRS::Plugin::SecurityStatus status;
    status.overallIntegrity = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    status.overallAuthenticity = LibreSCRS::Plugin::SecurityCheck::Status::Failed;
    status.overallGenuineness = LibreSCRS::Plugin::SecurityCheck::Status::NotSupported;

    LibreSCRS::Plugin::SecurityCheck check;
    check.checkId = "hash_dg1";
    check.category = LibreSCRS::Plugin::SecurityCategory::DataIntegrity;
    check.status = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    check.label = "DG1 Hash";
    check.detail = "Hash matches SOD";
    status.checks.push_back(check);

    check.checkId = "ds_cert";
    check.category = LibreSCRS::Plugin::SecurityCategory::Authenticity;
    check.status = LibreSCRS::Plugin::SecurityCheck::Status::Failed;
    check.label = "DS Certificate";
    check.detail = "Certificate expired";
    status.checks.push_back(check);

    widget.setSecurityStatus(status);
    // Widget renders without crash with detail checks
}

TEST_F(SecurityStatusWidgetTest, UpdateStatusTwice)
{
    SecurityStatusWidget widget;

    LibreSCRS::Plugin::SecurityStatus status1;
    status1.overallIntegrity = LibreSCRS::Plugin::SecurityCheck::Status::NotPerformed;
    widget.setSecurityStatus(status1);

    LibreSCRS::Plugin::SecurityStatus status2;
    status2.overallIntegrity = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    status2.overallAuthenticity = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    status2.overallGenuineness = LibreSCRS::Plugin::SecurityCheck::Status::Passed;
    widget.setSecurityStatus(status2);
    // Widget handles status update without crash
}
