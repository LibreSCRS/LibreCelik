// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Offscreen GUI coverage for the pieces the main window composes.
///
/// The window itself stays OUT of this binary — it drags the whole
/// application in with it. What the window is made of does not: the guided
/// presence panel and the streamed-group dispatch path are both reusable,
/// both driven by the scripted fakes, and both are what actually breaks when
/// the agent seam moves. The F5 feature gate is exercised through the
/// PRODUCTION helper (`requestOptionalSections`), never a test-local restating
/// of the predicate — a gate that exists only inside its own test proves
/// nothing about the window that ships.

#include "agent/agentstatewidget.h"
#include "agent/cardcontroller.h"
#include "agent/errortext.h"
#include "agent/optionalsections.h"
#include "agent/plugintyperesolution.h"
#include "fake_gateway/fakeagentgateway.h"
#include "fake_gateway/fakecardcontroller.h"
#include "mockwidgetplugin.h"

#include <LibreSCRS/AgentClient/CallError.h>
#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QLabel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QWidget>

#include <gtest/gtest.h>

using librecelik::agent::AgentStateWidget;
using librecelik::test::agent::FakeAgentGateway;
using librecelik::test::agent::FakeCardController;

namespace {

/// The shared offscreen application host. Every case here constructs real
/// widgets, and a widget built without a QApplication aborts the process
/// under the offscreen platform — so the fixture owns one and both suites
/// derive it. The instance guard keeps it to exactly one per binary.
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

QString stateMessage(AgentStateWidget& w)
{
    auto* label = w.findChild<QLabel*>(QStringLiteral("agentStateMessage"));
    return label ? label->text() : QString();
}

} // namespace

class AgentStateWidgetTest : public OffscreenGuiTest
{};
class MainFlowTest : public OffscreenGuiTest
{};

TEST_F(AgentStateWidgetTest, RendersDistinctTextPerPresenceState)
{
    using librecelik::agent::PresenceState;
    AgentStateWidget w;
    w.setState(PresenceState::AgentMissing, false);
    const QString missing = stateMessage(w);
    w.setState(PresenceState::AgentUnavailable, false);
    const QString unavailable = stateMessage(w);
    w.setState(PresenceState::Ready, false);
    const QString ready = stateMessage(w);
    EXPECT_FALSE(missing.isEmpty());
    EXPECT_FALSE(unavailable.isEmpty());
    // Ready is DESIGNED silent — the panel hides itself and the window's own
    // "insert a card" copy speaks instead, so the panel never says the same
    // thing twice. There is no lc-agent-state-ready id and none may be minted.
    EXPECT_TRUE(ready.isEmpty());
    EXPECT_NE(missing, unavailable);
    EXPECT_NE(unavailable, ready);
}

TEST_F(MainFlowTest, StreamedGroupsReachThePluginWidgetInOrder)
{
    using namespace LibreSCRS::AgentClient;
    FakeCardController ctrl;
    ctrl.scriptedCardType = QStringLiteral("mock");
    FieldGroup a;
    a.key = QStringLiteral("personal");
    FieldGroup b;
    b.key = QStringLiteral("address");
    ctrl.scriptedGroups = {a, b};
    QStringList delivered;
    // The dispatch glue under test: groupReady -> plugin->addGroup, the same
    // connect body the window installs; the v2 mock plugin records the keys.
    MockWidgetPlugin plugin;
    QWidget* shell = plugin.createEmptyWidget(nullptr);
    ASSERT_NE(shell, nullptr) << "the streaming shell must be real — a null shell would make this case vacuous";
    QObject::connect(&ctrl, &librecelik::agent::CardController::groupReady, [&](const FieldGroup& g) {
        plugin.addGroup(g, shell);
        delivered << g.key;
    });
    ctrl.startRead();
    EXPECT_EQ(delivered, (QStringList{QStringLiteral("personal"), QStringLiteral("address")}));
    EXPECT_EQ(plugin.recordedGroupKeys(), delivered);
    delete shell;
}

TEST_F(MainFlowTest, ScriptedErrorCarriesTheLocalizedCatalogText)
{
    using namespace LibreSCRS::AgentClient;
    FakeCardController ctrl;
    ctrl.scriptedError = librecelik::agent::errorText(ErrorCode::CardRemoved, CallError::None, {}, {});
    QString seen;
    QObject::connect(&ctrl, &librecelik::agent::CardController::errorOccurred, [&](const QString& msg) { seen = msg; });
    ctrl.failNextRead = true;
    ctrl.startRead();
    EXPECT_EQ(seen, librecelik::agent::errorText(ErrorCode::CardRemoved, CallError::None, {}, {}));
}

TEST_F(MainFlowTest, StreamedReadDeliversTheMergedPhotoGroupBeforeIdentityReady)
{
    using namespace LibreSCRS::AgentClient;
    // The D10 regression the fakes must mirror (the live read contract): when
    // the script carries a photo, a FINAL groupReady with key "photo"
    // precedes identityReady — streaming plugins render the portrait too.
    FakeCardController ctrl;
    FieldGroup personal;
    personal.key = QStringLiteral("personal");
    FieldGroup photo;
    photo.key = QStringLiteral("photo");
    ctrl.scriptedGroups = {personal};
    ctrl.scriptedPhotoGroup = photo; // fake replays the merge emission
    QStringList order;
    QObject::connect(&ctrl, &librecelik::agent::CardController::groupReady,
                     [&](const FieldGroup& g) { order << g.key; });
    bool identityAfterPhoto = false;
    QObject::connect(&ctrl, &librecelik::agent::CardController::identityReady,
                     [&](const auto&) { identityAfterPhoto = order.contains(QStringLiteral("photo")); });
    ctrl.startRead();
    EXPECT_EQ(order, (QStringList{QStringLiteral("personal"), QStringLiteral("photo")}));
    EXPECT_TRUE(identityAfterPhoto);
}

TEST_F(MainFlowTest, GuiPluginKeyResolutionCoversTheUnresolvableCard)
{
    // Drives the PRODUCTION helper (plugintyperesolution.h) — the
    // requestOptionalSections pattern: the decision the window makes must not
    // exist only as untested window glue. The regression this pins: a
    // CardEdge signing token kept multiple agent-side candidate
    // plugins, so CardType stayed "" and Capabilities carried the candidate
    // INTERSECTION — Pki|PinManagement, no IdentityData. The agent resolves a
    // card's type authoritatively ONLY from a completed ReadIdentity/GetPhoto
    // (Card1.xml), and refuses both verbs without IdentityData — so waiting
    // for cardTypeResolved was provably unbounded and the page spun forever.
    using librecelik::agent::effectiveGuiPluginKey;
    using librecelik::agent::kGenericTokenCardType;
    namespace Cap = LibreSCRS::AgentClient::Cap;

    // A resolved type always wins, regardless of caps or feature level.
    EXPECT_EQ(effectiveGuiPluginKey(true, Cap::Pki, QStringLiteral("rs-eid")), QStringLiteral("rs-eid"));
    // Pre-card-type agent, PKI surface: the historical middleware-path
    // fallback — render the generic token page rather than nothing.
    EXPECT_EQ(effectiveGuiPluginKey(false, Cap::Pki, QString()), kGenericTokenCardType);
    // THE BENCH CASE: card-type-capable agent, unresolved type, PKI without
    // IdentityData — resolution can never happen, so waiting is wrong; the
    // card is a PKI surface and must fall back to the generic token page.
    EXPECT_EQ(effectiveGuiPluginKey(true, Cap::Pki | Cap::PinManagement, QString()), kGenericTokenCardType);
    // Card-type-capable agent, unresolved type, IdentityData present: the
    // identity read WILL resolve the type — waiting is correct, render later.
    EXPECT_EQ(effectiveGuiPluginKey(true, Cap::Pki | Cap::IdentityData, QString()), QString());
    // No PKI surface at all: nothing the generic token page could render.
    EXPECT_EQ(effectiveGuiPluginKey(true, Cap::PinManagement, QString()), QString());
}

TEST_F(MainFlowTest, LateTypeResolutionStartsTheReadTheFallbackSkipped)
{
    // Companion decision to effectiveGuiPluginKey, same tested-helper rule.
    // The fallback renders the generic token page IMMEDIATELY when
    // resolution cannot happen (no IdentityData means the identity read is
    // never started). An agent that resolves the type later anyway — a
    // restart re-exports the seated card typeless with the candidate
    // capability INTERSECTION until its own probe completes — must get the
    // read the add-time decision skipped: identityReady's rebuild is what
    // re-plugins the page off the provisional token rendering. A read the
    // window already started must never be started twice by the same event.
    using librecelik::agent::lateResolutionStartsRead;
    namespace Cap = LibreSCRS::AgentClient::Cap;

    // THE BENCH CASE: fallback page (read never started), late resolution
    // re-exported the full family capability set — start the read now.
    EXPECT_TRUE(lateResolutionStartsRead(false, Cap::Pki | Cap::IdentityData));
    // Fallback page, resolved card still without IdentityData: there is no
    // read to start; the token page IS that card's whole surface.
    EXPECT_FALSE(lateResolutionStartsRead(false, Cap::Pki | Cap::PinManagement));
    // The ordinary multi-candidate wait: the running read is what resolved
    // the type — a second startRead would double-read the card.
    EXPECT_FALSE(lateResolutionStartsRead(true, Cap::Pki | Cap::IdentityData));
    EXPECT_FALSE(lateResolutionStartsRead(true, Cap::Pki));
}

TEST_F(MainFlowTest, TokenInfoAbsentFeatureIsSilentlyHiddenNeverAnError)
{
    // Old-agent honesty: the client gates on the feature token. This drives
    // the PRODUCTION gate — librecelik::agent::requestOptionalSections
    // (optionalsections.h, the exact code the window calls) — never a
    // test-local re-statement of the predicate.
    FakeAgentGateway gw;
    gw.scriptedFeatures = QStringList{}; // pre-feature agent
    FakeCardController ctrl;
    gw.registerCardController(QStringLiteral("card-1"), &ctrl);
    int errors = 0;
    QObject::connect(&ctrl, &librecelik::agent::CardController::errorOccurred, [&](const QString&) { ++errors; });
    const auto absent = librecelik::agent::requestOptionalSections(gw, ctrl);
    EXPECT_FALSE(absent.tokenInfo);
    EXPECT_FALSE(absent.credentials);
    EXPECT_EQ(errors, 0);
    EXPECT_FALSE(ctrl.callLog.contains(QStringLiteral("requestTokenInfo")));
    EXPECT_FALSE(ctrl.callLog.contains(QStringLiteral("requestCredentials")));
    // Non-vacuous both ways: with the tokens present the SAME helper
    // dispatches both verbs and reports both surfaces visible.
    gw.scriptedFeatures = QStringList{QStringLiteral("token-info"), QStringLiteral("credentials")};
    const auto present = librecelik::agent::requestOptionalSections(gw, ctrl);
    EXPECT_TRUE(present.tokenInfo);
    EXPECT_TRUE(present.credentials);
    EXPECT_TRUE(ctrl.callLog.contains(QStringLiteral("requestTokenInfo")));
    EXPECT_TRUE(ctrl.callLog.contains(QStringLiteral("requestCredentials")));
    EXPECT_EQ(errors, 0);
}
