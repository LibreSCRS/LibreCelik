// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include <gtest/gtest.h>
#include <QApplication>
#include "utils/collapsiblesection.h"

class CollapsibleSectionTest : public ::testing::Test
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
QApplication* CollapsibleSectionTest::app = nullptr;

TEST_F(CollapsibleSectionTest, DefaultHeaderColorIsTeal)
{
    CollapsibleSection section("Test");
    EXPECT_EQ(section.headerColor(), QColor(61, 140, 149));
}

TEST_F(CollapsibleSectionTest, SetHeaderColorChangesColor)
{
    CollapsibleSection section("Test");
    QColor navy(34, 86, 117);
    section.setHeaderColor(navy);
    EXPECT_EQ(section.headerColor(), navy);
}

TEST_F(CollapsibleSectionTest, HeaderColorInConstructor)
{
    QColor orange(230, 135, 60);
    CollapsibleSection section("Test", orange);
    EXPECT_EQ(section.headerColor(), orange);
}
