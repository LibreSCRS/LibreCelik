// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me

#include "document/emrtd/emrtdauthwidget.h"

#include <QApplication>
#include <QDateEdit>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalSpy>
#include <QTabWidget>
#include <QTest>
#include <gtest/gtest.h>

class EMRTDAuthWidgetTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        widget = new EMRTDAuthWidget();
        authButton = widget->findChild<QPushButton*>();
        ASSERT_NE(authButton, nullptr);
    }

    void TearDown() override { delete widget; }

    EMRTDAuthWidget* widget = nullptr;
    QPushButton* authButton = nullptr;
};

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST_F(EMRTDAuthWidgetTest, AuthButtonDisabledByDefault)
{
    EXPECT_FALSE(authButton->isEnabled());
}

TEST_F(EMRTDAuthWidgetTest, CANTabValidationRequires6Digits)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();
    ASSERT_NE(tabWidget, nullptr);
    tabWidget->setCurrentIndex(0); // CAN tab

    auto* canTab = tabWidget->widget(0);
    auto* canEdit = canTab->findChild<QLineEdit*>();
    ASSERT_NE(canEdit, nullptr);

    canEdit->setText("12345");
    EXPECT_FALSE(authButton->isEnabled());

    canEdit->setText("123456");
    EXPECT_TRUE(authButton->isEnabled());

    canEdit->setText("12345");
    EXPECT_FALSE(authButton->isEnabled());

    canEdit->setText("12ab56");
    EXPECT_FALSE(authButton->isEnabled());
}

TEST_F(EMRTDAuthWidgetTest, MRZTabValidationRequiresAllFields)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();
    ASSERT_NE(tabWidget, nullptr);
    tabWidget->setCurrentIndex(1); // MRZ tab

    auto* mrzTab = tabWidget->widget(1);
    auto* docNumberEdit = mrzTab->findChild<QLineEdit*>();
    ASSERT_NE(docNumberEdit, nullptr);

    auto dateEdits = mrzTab->findChildren<QDateEdit*>();
    ASSERT_EQ(dateEdits.size(), 2);
    auto* dobEdit = dateEdits[0];
    auto* expiryEdit = dateEdits[1];

    // All defaults — button disabled
    EXPECT_FALSE(authButton->isEnabled());

    // Only doc number — still disabled (dates at sentinel)
    docNumberEdit->setText("AB123");
    EXPECT_FALSE(authButton->isEnabled());

    // Set DOB to a real date
    dobEdit->setDate(QDate(1996, 5, 15));
    EXPECT_FALSE(authButton->isEnabled());

    // Set expiry — now all fields filled
    expiryEdit->setDate(QDate(2028, 1, 1));
    EXPECT_TRUE(authButton->isEnabled());

    // Clear doc number — disabled again
    docNumberEdit->clear();
    EXPECT_FALSE(authButton->isEnabled());
}

TEST_F(EMRTDAuthWidgetTest, MRZTabDocNumberUppercaseAndMaxLength)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();
    tabWidget->setCurrentIndex(1);

    auto* mrzTab = tabWidget->widget(1);
    auto* docNumberEdit = mrzTab->findChild<QLineEdit*>();
    ASSERT_NE(docNumberEdit, nullptr);

    docNumberEdit->setText("ab123xyz9");
    EXPECT_EQ(docNumberEdit->text(), "AB123XYZ9");

    docNumberEdit->setText("AB123456789"); // 11 chars
    EXPECT_EQ(docNumberEdit->text().length(), 9);
}

TEST_F(EMRTDAuthWidgetTest, CANTabEmitsCANCredentials)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();
    tabWidget->setCurrentIndex(0);

    auto* canTab = tabWidget->widget(0);
    auto* canEdit = canTab->findChild<QLineEdit*>();
    canEdit->setText("123456");

    QSignalSpy spy(widget, &EMRTDAuthWidget::credentialsEntered);
    QTest::mouseClick(authButton, Qt::LeftButton);

    ASSERT_EQ(spy.count(), 1);
    auto creds = spy.first().first().value<QMap<QString, QString>>();
    EXPECT_EQ(creds.size(), 1);
    EXPECT_EQ(creds["can"], "123456");
}

TEST_F(EMRTDAuthWidgetTest, MRZTabEmitsThreeCredentialsInYYMMDD)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();
    tabWidget->setCurrentIndex(1);

    auto* mrzTab = tabWidget->widget(1);
    auto* docNumberEdit = mrzTab->findChild<QLineEdit*>();
    auto dateEdits = mrzTab->findChildren<QDateEdit*>();
    auto* dobEdit = dateEdits[0];
    auto* expiryEdit = dateEdits[1];

    docNumberEdit->setText("AB1234567");
    dobEdit->setDate(QDate(1996, 5, 15));
    expiryEdit->setDate(QDate(2028, 1, 1));

    QSignalSpy spy(widget, &EMRTDAuthWidget::credentialsEntered);
    QTest::mouseClick(authButton, Qt::LeftButton);

    ASSERT_EQ(spy.count(), 1);
    auto creds = spy.first().first().value<QMap<QString, QString>>();
    EXPECT_EQ(creds.size(), 3);
    EXPECT_EQ(creds["mrz_doc_number"], "AB1234567");
    EXPECT_EQ(creds["mrz_dob"], "960515");
    EXPECT_EQ(creds["mrz_expiry"], "280101");
}

TEST_F(EMRTDAuthWidgetTest, SetDefaultTabPACESupportedSelectsCAN)
{
    widget->setDefaultTab(true);
    auto* tabWidget = widget->findChild<QTabWidget*>();
    EXPECT_EQ(tabWidget->currentIndex(), 0);
}

TEST_F(EMRTDAuthWidgetTest, SetDefaultTabNoPACESelectsMRZ)
{
    widget->setDefaultTab(false);
    auto* tabWidget = widget->findChild<QTabWidget*>();
    EXPECT_EQ(tabWidget->currentIndex(), 1);
}

TEST_F(EMRTDAuthWidgetTest, TabSwitchRevalidatesButton)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();

    // Fill CAN tab
    tabWidget->setCurrentIndex(0);
    auto* canTab = tabWidget->widget(0);
    auto* canEdit = canTab->findChild<QLineEdit*>();
    canEdit->setText("123456");
    EXPECT_TRUE(authButton->isEnabled());

    // Switch to MRZ — button should disable (MRZ fields empty)
    tabWidget->setCurrentIndex(1);
    EXPECT_FALSE(authButton->isEnabled());

    // Switch back to CAN — button should re-enable (CAN still filled)
    tabWidget->setCurrentIndex(0);
    EXPECT_TRUE(authButton->isEnabled());
}

TEST_F(EMRTDAuthWidgetTest, MRZTabYYMMDDConversionEdgeCases)
{
    auto* tabWidget = widget->findChild<QTabWidget*>();
    tabWidget->setCurrentIndex(1);

    auto* mrzTab = tabWidget->widget(1);
    auto* docNumberEdit = mrzTab->findChild<QLineEdit*>();
    auto dateEdits = mrzTab->findChildren<QDateEdit*>();

    docNumberEdit->setText("X1");
    dateEdits[0]->setDate(QDate(2005, 12, 31)); // DOB -> "051231"
    dateEdits[1]->setDate(QDate(2030, 6, 1));   // Expiry -> "300601"

    QSignalSpy spy(widget, &EMRTDAuthWidget::credentialsEntered);
    QTest::mouseClick(authButton, Qt::LeftButton);

    ASSERT_EQ(spy.count(), 1);
    auto creds = spy.first().first().value<QMap<QString, QString>>();
    EXPECT_EQ(creds["mrz_dob"], "051231");
    EXPECT_EQ(creds["mrz_expiry"], "300601");
}
