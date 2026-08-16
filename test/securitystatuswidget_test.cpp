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
