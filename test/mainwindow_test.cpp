// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen coverage for the main window itself.
///
/// The window used to sit outside every test binary: naming it in a target
/// meant restating the whole application's source list, so its parts were
/// covered and their composition was not. The application object library
/// removes that obstacle — this binary links the same objects the executable
/// ships — and the window takes its gateway from its caller, so a scripted
/// one can drive it with no agent, no D-Bus and no card.

#include "librecelik.h"

#include "agent/agentgateway.h"
#include "agent/agentstatewidget.h"
#include "certificate/certificateviewerdlg.h"
#include "document/tokensection.h"
#include "fake_gateway/fakeagentgateway.h"
#include "fake_gateway/fakecardcontroller.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QMenu>
#include <QPointer>
#include <QTimer>
#include <QTreeWidget>
#include <QLabel>
#include <QPushButton>
#include <QString>

#include <gtest/gtest.h>

#include <memory>

using librecelik::agent::AgentStateWidget;
using librecelik::agent::PresenceState;
using librecelik::test::agent::FakeAgentGateway;
using librecelik::test::agent::FakeCardController;

namespace {

/// The shared offscreen application host. Every case here constructs real
/// widgets, and a widget built without a QApplication aborts the process under
/// the offscreen platform — so the fixture owns one. The instance guard keeps
/// it to exactly one per binary.
class OffscreenGuiTest : public ::testing::Test
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
    }
    static QApplication* app;
};
QApplication* OffscreenGuiTest::app = nullptr;

} // namespace

class MainWindowTest : public OffscreenGuiTest
{};

/// The retry affordance the guided panel offers has to reach the gateway this
/// window actually holds, and has to ask it to START the agent: an installed
/// agent nobody started stays stopped however often the window only looks. A
/// window that built its own gateway would ask that one instead and leave the
/// scripted log unchanged, which is the whole point of the assertion: it fails
/// on the composition, not on the panel.
TEST_F(MainWindowTest, RetryFromTheGuidedPanelStartsTheAgentThroughTheInjectedGateway)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    FakeAgentGateway* scripted = gateway.get();
    scripted->scriptedPresence = PresenceState::AgentUnavailable;
    scripted->scriptedStartResult = LibreSCRS::AgentClient::AgentStartResult::Failed;

    LibreCelik window(std::move(gateway));
    // Opening the window already asked once, on its own.
    EXPECT_EQ(scripted->callLog.count(QStringLiteral("requestAgentStart")), 1);

    auto* panel = window.findChild<AgentStateWidget*>();
    ASSERT_NE(panel, nullptr);
    auto* retry = panel->findChild<QPushButton*>();
    ASSERT_NE(retry, nullptr);

    scripted->scriptedStartResult = LibreSCRS::AgentClient::AgentStartResult::Running;
    retry->click();

    EXPECT_EQ(scripted->callLog.count(QStringLiteral("requestAgentStart")), 2);
    EXPECT_EQ(scripted->presence(), PresenceState::Ready);
    EXPECT_TRUE(panel->isHidden());
}

/// The panel's "Try again" is held while the client is still answering a start
/// (on Linux it waits for the bus and keeps the window live meanwhile), and
/// comes back once the start has its answer.
TEST_F(MainWindowTest, TryAgainIsHeldWhileTheAgentIsBeingStarted)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    FakeAgentGateway* scripted = gateway.get();
    scripted->scriptedPresence = PresenceState::AgentUnavailable;
    scripted->scriptedStartResult = LibreSCRS::AgentClient::AgentStartResult::Failed;

    LibreCelik window(std::move(gateway));
    ASSERT_EQ(scripted->callLog.count(QStringLiteral("requestAgentStart")), 1);
    auto* panel = window.findChild<AgentStateWidget*>();
    ASSERT_NE(panel, nullptr);
    auto* retry = panel->findChild<QPushButton*>();
    ASSERT_NE(retry, nullptr);
    ASSERT_TRUE(retry->isEnabled());

    bool enabledDuringTheStart = true;
    scripted->duringAgentStart = [&] {
        enabledDuringTheStart = retry->isEnabled();
        scripted->duringAgentStart = nullptr;
        retry->click();
    };
    retry->click();

    EXPECT_FALSE(enabledDuringTheStart);
    EXPECT_EQ(scripted->callLog.count(QStringLiteral("requestAgentStart")), 2);
    EXPECT_TRUE(retry->isEnabled());
}

/// While the agent is out of reach the window must not claim that no card was
/// detected: it cannot see the readers to know. The guided panel is the only
/// honest copy in that state, so the empty-state text is suppressed.
TEST_F(MainWindowTest, SuppressesTheNoCardCopyWhileTheAgentIsOutOfReach)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    gateway->scriptedPresence = PresenceState::AgentMissing;

    LibreCelik window(std::move(gateway));

    auto* emptyStateCopy = window.findChild<QLabel*>(QStringLiteral("label_3"));
    ASSERT_NE(emptyStateCopy, nullptr);
    EXPECT_TRUE(emptyStateCopy->isHidden());
}

/// The mirror of the case above: once the agent answers, the window is again
/// entitled to say that no card is present, so the copy comes back.
TEST_F(MainWindowTest, RestoresTheNoCardCopyOnceTheAgentAnswers)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    gateway->scriptedPresence = PresenceState::Ready;

    LibreCelik window(std::move(gateway));

    auto* emptyStateCopy = window.findChild<QLabel*>(QStringLiteral("label_3"));
    ASSERT_NE(emptyStateCopy, nullptr);
    EXPECT_FALSE(emptyStateCopy->isHidden());
}

/// A card that wants an unlock the window cannot name (a method added to the
/// wire after this build) is announced as waiting for that unlock — not as an
/// ordinary read, which would tell the holder nothing is asked of them while
/// the agent's prompt is about to appear.
TEST_F(MainWindowTest, ACardWantingAnUnlockIsAnnouncedAsWaitingForIt)
{
    auto gateway = std::make_unique<FakeAgentGateway>();
    gateway->scriptedPresence = PresenceState::Ready;
    gateway->scriptedReaders = {{QStringLiteral("r1"), QStringLiteral("Reader"), true, QStringLiteral("c1")}};
    auto* controller = new FakeCardController(gateway.get());
    controller->scriptedCardId = QStringLiteral("c1");
    controller->scriptedCaps = LibreSCRS::AgentClient::Cap::IdentityData;
    controller->scriptedUnlockRequired = true;
    controller->holdRead = true;
    gateway->registerCardController(QStringLiteral("c1"), controller);

    LibreCelik window(std::move(gateway));

    bool announced = false;
    for (const QLabel* label : window.findChildren<QLabel*>()) {
        announced = announced || label->text() == QStringLiteral("lc-agent-awaiting-preauth");
    }
    EXPECT_TRUE(announced);
    EXPECT_TRUE(controller->callLog.contains(QStringLiteral("startRead")));
}

namespace {

/// A window holding one PKI-only card ("c1") with one certificate, so its page
/// is the token page and carries the certificate section.
struct PkiCardWindow
{
    FakeAgentGateway* gateway = nullptr;
    std::unique_ptr<LibreCelik> window;

    PkiCardWindow()
    {
        auto owned = std::make_unique<FakeAgentGateway>();
        gateway = owned.get();
        gateway->scriptedPresence = PresenceState::Ready;
        gateway->scriptedReaders = {{QStringLiteral("r1"), QStringLiteral("Reader"), true, QStringLiteral("c1")}};
        auto* controller = new FakeCardController(gateway);
        controller->scriptedCardId = QStringLiteral("c1");
        controller->scriptedReaderId = QStringLiteral("r1");
        controller->scriptedCaps = LibreSCRS::AgentClient::Cap::Pki;
        LibreSCRS::AgentClient::CertificateInfo cert;
        cert.id = QStringLiteral("cert-1");
        cert.subject = QStringLiteral("CN=Holder");
        controller->scriptedCerts = {cert};
        gateway->registerCardController(QStringLiteral("c1"), controller);
        window = std::make_unique<LibreCelik>(std::move(owned));
    }

    [[nodiscard]] TokenSection* section() const
    {
        return window->findChild<TokenSection*>();
    }
};

/// Runs what a return to the top-level event loop would run: queued calls and
/// the deferred deletions posted while handling them.
void settle()
{
    for (int round = 0; round < 3; ++round) {
        QCoreApplication::processEvents();
        QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    }
}

/// A safety net for the defect these cases guard: a dialog or menu that
/// blocks in its own loop would otherwise hang the suite instead of failing it.
void closeAnyModalSoon()
{
    QTimer::singleShot(3000, [] {
        if (QWidget* modal = QApplication::activeModalWidget())
            modal->close();
        if (QWidget* popup = QApplication::activePopupWidget())
            popup->close();
    });
}

} // namespace

/// Pulling the card while its certificate viewer is open destroys the card's
/// page. The viewer must survive that as an ordinary heap window that closes
/// itself — not as an object on a stack frame or among the page's children,
/// which the page teardown would delete under the dialog's own loop.
TEST_F(MainWindowTest, PullingTheCardClosesItsCertificateViewer)
{
    PkiCardWindow fixture;
    TokenSection* section = fixture.section();
    ASSERT_NE(section, nullptr);

    LibreSCRS::AgentClient::CertificateInfo cert;
    cert.id = QStringLiteral("cert-1");
    closeAnyModalSoon();
    emit section->certificateDetailsRequested(cert);
    settle();

    QPointer<CertificateViewerDlg> viewer = fixture.window->findChild<CertificateViewerDlg*>();
    ASSERT_FALSE(viewer.isNull());
    EXPECT_TRUE(viewer->isVisible());

    fixture.gateway->removeCard(QStringLiteral("c1"));
    settle();

    EXPECT_TRUE(viewer.isNull());
    EXPECT_EQ(fixture.window->findChild<TokenSection*>(), nullptr);
}

/// The same teardown with the certificate's context menu open: the menu is a
/// child of the section the page teardown deletes, so it has to be a heap
/// object that goes with it, not one on the stack of the handler that showed it.
TEST_F(MainWindowTest, PullingTheCardWithTheCertificateMenuOpenTearsDownCleanly)
{
    PkiCardWindow fixture;
    TokenSection* section = fixture.section();
    ASSERT_NE(section, nullptr);
    auto* tree = section->findChild<QTreeWidget*>();
    ASSERT_NE(tree, nullptr);
    QTreeWidgetItem* certItem = nullptr;
    for (QTreeWidgetItemIterator it(tree); *it != nullptr; ++it) {
        if ((*it)->text(0).contains(QStringLiteral("Holder")) || (*it)->text(1).contains(QStringLiteral("Holder"))) {
            certItem = *it;
            break;
        }
    }
    ASSERT_NE(certItem, nullptr);
    certItem->parent()->setExpanded(true);
    section->setExpanded(true);
    fixture.window->show();
    settle();

    closeAnyModalSoon();
    emit tree->customContextMenuRequested(tree->visualItemRect(certItem).center());
    settle();
    QPointer<QMenu> menu = section->findChild<QMenu*>();
    ASSERT_FALSE(menu.isNull());

    fixture.gateway->removeCard(QStringLiteral("c1"));
    settle();

    EXPECT_TRUE(menu.isNull());
    EXPECT_EQ(fixture.window->findChild<TokenSection*>(), nullptr);
}
