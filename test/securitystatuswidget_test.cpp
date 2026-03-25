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
    plugin::SecurityStatus status;
    status.overallIntegrity = plugin::SecurityCheck::PASSED;
    status.overallAuthenticity = plugin::SecurityCheck::PASSED;
    status.overallGenuineness = plugin::SecurityCheck::NOT_PERFORMED;
    widget.setSecurityStatus(status);
    // Widget renders without crash
}

TEST_F(SecurityStatusWidgetTest, RendersWithDetailChecks)
{
    SecurityStatusWidget widget;
    plugin::SecurityStatus status;
    status.overallIntegrity = plugin::SecurityCheck::PASSED;
    status.overallAuthenticity = plugin::SecurityCheck::FAILED;
    status.overallGenuineness = plugin::SecurityCheck::NOT_SUPPORTED;

    plugin::SecurityCheck check;
    check.checkId = "hash_dg1";
    check.category = "data_integrity";
    check.status = plugin::SecurityCheck::PASSED;
    check.label = "DG1 Hash";
    check.detail = "Hash matches SOD";
    status.checks.push_back(check);

    check.checkId = "ds_cert";
    check.category = "data_authenticity";
    check.status = plugin::SecurityCheck::FAILED;
    check.label = "DS Certificate";
    check.detail = "Certificate expired";
    status.checks.push_back(check);

    widget.setSecurityStatus(status);
    // Widget renders without crash with detail checks
}

TEST_F(SecurityStatusWidgetTest, UpdateStatusTwice)
{
    SecurityStatusWidget widget;

    plugin::SecurityStatus status1;
    status1.overallIntegrity = plugin::SecurityCheck::NOT_PERFORMED;
    widget.setSecurityStatus(status1);

    plugin::SecurityStatus status2;
    status2.overallIntegrity = plugin::SecurityCheck::PASSED;
    status2.overallAuthenticity = plugin::SecurityCheck::PASSED;
    status2.overallGenuineness = plugin::SecurityCheck::PASSED;
    widget.setSecurityStatus(status2);
    // Widget handles status update without crash
}
