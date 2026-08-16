// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen coverage for the settings dialog over the agent's Config1.
///
/// The operation-affecting preferences are the AGENT's, not a file this
/// process owns: the level a signature defaults to, the timestamp authorities
/// it may use, the trusted lists it validates against. So what there is to
/// assert is what the dialog READS from the agent's snapshot, what it WRITES
/// back through `setConfigValue` (in the wire's own spelling, not the combo's),
/// which keys a per-tab "restore defaults" hands to `resetConfigValue`, what it
/// SAYS when a write is refused — and that a refusal is said exactly once,
/// never retried. The file-only cache-directory control is gone, and the
/// operation-backed tabs go dark when no agent is there to keep them.
///
/// Nothing here dials anything: the gateway is the campaign's scripted fake.

#include "settings/settingsdialog.h"

#include "fake_gateway/fakeagentgateway.h"
#include "settings/tlitemdelegate.h"

#include <LibreSCRS/AgentClient/SyncError.h>

#include <QApplication>
#include <QComboBox>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTabWidget>
#include <QVariant>

#include <gtest/gtest.h>

namespace {

using librecelik::test::agent::FakeAgentGateway;

/// The dialog commits on OK; the button carries an object name precisely so a
/// test can press the same thing a human presses.
void invokeSave(SettingsDialog& dlg)
{
    auto* ok = dlg.findChild<QPushButton*>(QStringLiteral("okButton"));
    ASSERT_NE(ok, nullptr);
    ok->click();
}

QString statusLabelText(SettingsDialog& dlg)
{
    auto* label = dlg.findChild<QLabel*>(QStringLiteral("statusLabel"));
    return label != nullptr ? label->text() : QString();
}

/// Replace the trust tab's list content with @p urls, leaving the translated
/// "add" sentinel row where the delegate expects it — exactly the shape
/// `onTlAddRequested()` leaves behind after a human adds a list.
void setTrustTabTslList(SettingsDialog& dlg, const QStringList& urls)
{
    auto* list = dlg.findChild<QListWidget*>(QStringLiteral("tlList"));
    ASSERT_NE(list, nullptr);
    for (int row = list->count() - 1; row >= 0; --row) {
        if (list->item(row)->data(TlItemDelegate::TypeRole).toString() != QStringLiteral("add")) {
            delete list->takeItem(row);
        }
    }
    for (const QString& url : urls) {
        auto* item = new QListWidgetItem(url);
        item->setData(TlItemDelegate::TypeRole, QStringLiteral("custom"));
        item->setData(TlItemDelegate::IsLotlRole, false);
        item->setData(TlItemDelegate::EagerRole, true);
        list->insertItem(list->count() - 1, item);
    }
}

void clickSigningTabRestoreDefaults(SettingsDialog& dlg)
{
    auto* button = dlg.findChild<QPushButton*>(QStringLiteral("signingRestoreDefaultsButton"));
    ASSERT_NE(button, nullptr);
    button->click();
}

} // namespace

/// The LC widget-test idiom: one QApplication per binary, owned statically by
/// the fixture, because a widget built without one aborts the process under the
/// offscreen platform. The instance guard keeps it to exactly one.
class SettingsConfig1Test : public ::testing::Test
{
protected:
    void SetUp() override
    {
        if (!QApplication::instance()) {
            // Static: QApplication keeps a reference to argc for its lifetime,
            // so it must outlive this scope.
            static int argc = 0;
            app = new QApplication(argc, nullptr);
        }
        // The dialog still keeps the language and the default output folder in
        // QSettings; keep that half off the developer's real configuration.
        QStandardPaths::setTestModeEnabled(true);
    }

    static QApplication* app;
};

QApplication* SettingsConfig1Test::app = nullptr;

TEST_F(SettingsConfig1Test, LevelComboWritesWireToken)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.config[QStringLiteral("DefaultLevel")] = QStringLiteral("b-b");
    SettingsDialog dlg(&gw);
    auto* combo = dlg.findChild<QComboBox*>(QStringLiteral("defaultLevelCombo"));
    ASSERT_NE(combo, nullptr);
    combo->setCurrentIndex(combo->findData(QStringLiteral("B_T")));
    invokeSave(dlg); // clicks the OK button by objectName
    ASSERT_FALSE(gw.configWrites.isEmpty());
    EXPECT_EQ(gw.configWrites.last(), qMakePair(QStringLiteral("DefaultLevel"), QVariant(QStringLiteral("b-t"))));
}

TEST_F(SettingsConfig1Test, TslCacheDirControlIsGone)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);
    EXPECT_EQ(dlg.findChild<QLineEdit*>(QStringLiteral("cacheDir")), nullptr);
}

TEST_F(SettingsConfig1Test, OperationTabsDisabledWhileAgentMissing)
{
    FakeAgentGateway gw; // presence defaults to AgentMissing
    SettingsDialog dlg(&gw);
    auto* tabs = dlg.findChild<QTabWidget*>();
    ASSERT_NE(tabs, nullptr);
    EXPECT_TRUE(tabs->isTabEnabled(0));  // General (language) stays usable
    EXPECT_FALSE(tabs->isTabEnabled(1)); // Signing
    EXPECT_FALSE(tabs->isTabEnabled(2)); // Trust
}

TEST_F(SettingsConfig1Test, RestoreDefaultsResetsEveryKeyOfTheTab)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    SettingsDialog dlg(&gw);
    clickSigningTabRestoreDefaults(dlg); // helper: findChild by objectName
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("DefaultLevel")));
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("TsaUrls")));
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("DefaultReason")));
    EXPECT_TRUE(gw.configResets.contains(QStringLiteral("DefaultLocation")));
}

TEST_F(SettingsConfig1Test, NotAuthorizedRefusalRendersOnceAndNeverReprompts)
{
    FakeAgentGateway gw;
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    gw.nextRefusal = LibreSCRS::AgentClient::SyncError::NotAuthorized;
    SettingsDialog dlg(&gw);
    setTrustTabTslList(dlg, {QStringLiteral("https://x/tl.xml")});
    invokeSave(dlg);
    EXPECT_EQ(gw.configWrites.size(), 1); // exactly one attempt — no retry loop
    EXPECT_TRUE(statusLabelText(dlg).contains(qtTrId("lc-settings-config-unauthorized")));
}
