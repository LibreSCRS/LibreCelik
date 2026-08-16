// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen coverage for the secretless credential dialog.
///
/// The dialog collects NOTHING. Every secret this flow needs — the code in
/// force, the new one, a PUK — is asked for by the agent's own prompter, in
/// the agent's own process. So what there is to assert is what the dialog
/// OFFERS (which verbs, from which record), what it SAYS about an attempt the
/// agent answered, what it SENDS when a verb is chosen, and when it takes
/// itself off the screen. Every case reads that back off the widgets a holder
/// looks at.

#include "document/rs-eid/changepindlg.h"

#include "agent/errortext.h"
#include "fake_gateway/fakeagentgateway.h"

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/SignOptions.h>

#include <QApplication>
#include <QCheckBox>
#include <QDialog>
#include <QLabel>
#include <QPushButton>
#include <QSignalSpy>
#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

using namespace LibreSCRS::AgentClient;
using librecelik::test::agent::FakeAgentGateway;

namespace {

CredentialRecord userPin()
{
    CredentialRecord r;
    r.id = QStringLiteral("pin-1");
    r.label = QStringLiteral("User PIN");
    r.kind = CredentialKind::User;
    r.canChange = true;
    return r;
}

} // namespace

/// The LC widget-test idiom: one QApplication per binary, owned statically by
/// the fixture, because a widget built without one aborts the process under
/// the offscreen platform. The instance guard keeps it to exactly one even
/// though another suite in this binary installs its own.
class ChangePinDlgTest : public ::testing::Test
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
        installTranslatorOnce();
    }

    /// English catalog strings, once per binary. The retries line this suite
    /// reads substitutes the count into a TRANSLATED template — untranslated
    /// it renders the bare catalog id and the count disappears entirely, so
    /// the case would assert nothing about the number it is named after.
    static void installTranslatorOnce()
    {
        if (translator != nullptr)
            return;
        translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_en"), qmDir))
            << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
        QCoreApplication::installTranslator(translator);
    }

    static QApplication* app;
    static QTranslator* translator;
};

QApplication* ChangePinDlgTest::app = nullptr;
QTranslator* ChangePinDlgTest::translator = nullptr;

TEST_F(ChangePinDlgTest, VerbButtonsFollowTheCredentialRecord)
{
    ChangePinDlg dlg(userPin(), nullptr, {});
    auto* change = dlg.findChild<QPushButton*>(QStringLiteral("changeButton"));
    auto* unblock = dlg.findChild<QPushButton*>(QStringLiteral("unblockButton"));
    auto* activate = dlg.findChild<QPushButton*>(QStringLiteral("activateButton"));
    ASSERT_NE(change, nullptr);
    EXPECT_TRUE(change->isEnabled());
    EXPECT_TRUE(unblock == nullptr || !unblock->isVisibleTo(&dlg));
    EXPECT_TRUE(activate == nullptr || !activate->isVisibleTo(&dlg));
}

TEST_F(ChangePinDlgTest, InvalidPinOutcomeShowsRetriesLeft)
{
    ChangePinDlg dlg(userPin(), nullptr, {});
    PinResult res;
    res.outcome = CredentialOutcome::InvalidPin;
    res.retriesLeft = 2;
    dlg.onPinResult(res);
    auto* status = dlg.findChild<QLabel*>(QStringLiteral("statusLabel"));
    ASSERT_NE(status, nullptr);
    EXPECT_TRUE(status->text().contains(librecelik::agent::outcomeText(CredentialOutcome::InvalidPin)));
    EXPECT_TRUE(status->text().contains(QStringLiteral("2")));
}

TEST_F(ChangePinDlgTest, ActivateSendsActivateKeyOptionWhenChecked)
{
    CredentialRecord r = userPin();
    r.canChange = false;
    r.activatable = true;
    r.keyActivationPending = true;
    ChangePinDlg dlg(r, nullptr, {});
    qRegisterMetaType<ManagePinOptions>();
    QSignalSpy spy(&dlg, &ChangePinDlg::verbRequested);
    dlg.findChild<QCheckBox*>(QStringLiteral("activateKeyCheck"))->setChecked(true);
    dlg.findChild<QPushButton*>(QStringLiteral("activateButton"))->click();
    ASSERT_EQ(spy.count(), 1);
    const auto args = spy.takeFirst();
    EXPECT_EQ(args.at(0).toString(), QStringLiteral("pin-1"));
    EXPECT_EQ(args.at(1).value<PinVerb>(), PinVerb::ActivatePin);
    EXPECT_TRUE(args.at(2).value<ManagePinOptions>().activateKey);
}

TEST_F(ChangePinDlgTest, CardRemovalClosesTheDialog)
{
    // Modal hygiene owned by the dialog: removal (and agent loss, which
    // fans out cardRemoved) rejects it — no window glue in the loop.
    FakeAgentGateway gw;
    ChangePinDlg dlg(userPin(), &gw, QStringLiteral("card-1"));
    dlg.show();
    gw.removeCard(QStringLiteral("card-1"));
    EXPECT_EQ(dlg.result(), QDialog::Rejected);
    EXPECT_FALSE(dlg.isVisible());
}
