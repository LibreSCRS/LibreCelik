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
#include "agent/cardstatuspage.h"
#include "agent/errortext.h"
#include "agent/optionalsections.h"
#include "agent/plugintyperesolution.h"
#include "fake_gateway/fakeagentgateway.h"
#include "fake_gateway/fakecardcontroller.h"
#include "mockwidgetplugin.h"

#include <LibreSCRS/AgentClient/AgentCapabilities.h>
#include <LibreSCRS/AgentClient/CallError.h>
#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QApplication>
#include <QEvent>
#include <QLabel>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTranslator>
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

// A failed read costs the card its page, and only re-seating the card brings it
// back. That has to depend on WHICH failure: latching one the holder can clear
// strands them, because the message they are shown says "try again" while the
// surface that would let them is gone. Measured on a live agent — three entry
// windows allowed to expire, three retries, reader gone from the window.
TEST_F(MainFlowTest, AFailureTheHolderCanClearIsNotLatched)
{
    using namespace LibreSCRS::AgentClient;
    // The holder ran out of time, dismissed the prompt, or mistyped: asking
    // again is exactly the right move, so the page must survive.
    EXPECT_TRUE(librecelik::agent::isRetryableReadFailure(ErrorCode::EntryExpired));
    EXPECT_TRUE(librecelik::agent::isRetryableReadFailure(ErrorCode::CredentialWrong));
    EXPECT_TRUE(librecelik::agent::isRetryableReadFailure(ErrorCode::None));

    // Repeating these cannot help, so the retry-storm guard still latches.
    EXPECT_FALSE(librecelik::agent::isRetryableReadFailure(ErrorCode::UnsupportedCard));
    EXPECT_FALSE(librecelik::agent::isRetryableReadFailure(ErrorCode::ParseError));
    EXPECT_FALSE(librecelik::agent::isRetryableReadFailure(ErrorCode::CommunicationError));

    // A code this build has no name for is NOT retryable: the guard stays shut
    // until someone classifies it deliberately.
    EXPECT_FALSE(librecelik::agent::isRetryableReadFailure(static_cast<ErrorCode>(9999)));
}

// The message and the code travel together, so a window can act on the code
// while showing the message.
TEST_F(MainFlowTest, ScriptedErrorCarriesItsTaxonomyValueAlongsideTheText)
{
    using namespace LibreSCRS::AgentClient;
    FakeCardController ctrl;
    ctrl.scriptedError = librecelik::agent::errorText(ErrorCode::EntryExpired, CallError::None, {}, {});
    ctrl.scriptedErrorCode = ErrorCode::EntryExpired;
    ErrorCode seen = ErrorCode::None;
    QObject::connect(&ctrl, &librecelik::agent::CardController::errorOccurred,
                     [&](const QString&, ErrorCode code) { seen = code; });
    ctrl.failNextRead = true;
    ctrl.startRead();
    EXPECT_EQ(seen, ErrorCode::EntryExpired);
    EXPECT_TRUE(librecelik::agent::isRetryableReadFailure(seen));
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
    namespace Cap = LibreSCRS::AgentClient::Cap;
    FakeAgentGateway gw;
    gw.scriptedFeatures = QStringList{}; // pre-feature agent
    FakeCardController ctrl;
    // The positive arm below needs a card whose caps carry both surfaces —
    // the helper now mirrors the agent's per-card gate too (see the
    // caps-gate case), so a capless card would mask the feature dimension
    // this case pins.
    ctrl.scriptedCaps = Cap::Pki | Cap::PinManagement;
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

TEST_F(MainFlowTest, OptionalSectionsAreNeverDispatchedAgainstACardWithoutTheCapability)
{
    // The Leg-5 bench catch: a pure eMRTD (IdentityData|EmrtdCrypto, no Pki,
    // no PinManagement) against a feature-capable agent. The agent refuses
    // ReadTokenInfo without Pki and ListCredentials without PinManagement,
    // per card — and those refusals came back through errorOccurred while
    // the page was still the spinner, which the window reads as a failed
    // READ and releases the page the still-running identity read was about
    // to fill. The helper mirrors the agent's own per-card gate, so a verb
    // the card provably cannot answer is never dispatched at all.
    namespace Cap = LibreSCRS::AgentClient::Cap;
    FakeAgentGateway gw;
    gw.scriptedFeatures = QStringList{QStringLiteral("token-info"), QStringLiteral("credentials")};
    FakeCardController ctrl;
    ctrl.scriptedCaps = Cap::IdentityData | Cap::EmrtdCrypto; // the Czech-passport shape
    gw.registerCardController(QStringLiteral("card-1"), &ctrl);
    int errors = 0;
    QObject::connect(&ctrl, &librecelik::agent::CardController::errorOccurred, [&](const QString&) { ++errors; });
    const auto sections = librecelik::agent::requestOptionalSections(gw, ctrl);
    EXPECT_FALSE(sections.tokenInfo);
    EXPECT_FALSE(sections.credentials);
    EXPECT_FALSE(ctrl.callLog.contains(QStringLiteral("requestTokenInfo")));
    EXPECT_FALSE(ctrl.callLog.contains(QStringLiteral("requestCredentials")));
    EXPECT_EQ(errors, 0);
    // Split surfaces split the dispatch: Pki alone buys token info and
    // nothing else; PinManagement alone buys credentials and nothing else.
    FakeCardController pkiOnly;
    pkiOnly.scriptedCaps = Cap::Pki;
    const auto pki = librecelik::agent::requestOptionalSections(gw, pkiOnly);
    EXPECT_TRUE(pki.tokenInfo);
    EXPECT_FALSE(pki.credentials);
    FakeCardController pinOnly;
    pinOnly.scriptedCaps = Cap::PinManagement;
    const auto pin = librecelik::agent::requestOptionalSections(gw, pinOnly);
    EXPECT_FALSE(pin.tokenInfo);
    EXPECT_TRUE(pin.credentials);
}

// ---- the page a card gets when it has no readable surface -----------------

/// The status page substitutes the reader name and the ATR into TRANSLATED
/// templates, so an untranslated run would render the bare catalog ids and
/// drop both — the cases below would then assert nothing about the two facts
/// they exist for. The suite installs the English catalog itself rather than
/// relying on another suite in this binary having run first.
class CardStatusPageTest : public OffscreenGuiTest
{
protected:
    void SetUp() override
    {
        OffscreenGuiTest::SetUp();
        if (translator != nullptr)
            return;
        translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_en"), qmDir))
            << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
        QCoreApplication::installTranslator(translator);
    }
    static QTranslator* translator;
};
QTranslator* CardStatusPageTest::translator = nullptr;

namespace {

QString labelText(const QWidget& page, const char* objectName)
{
    auto* label = page.findChild<QLabel*>(QLatin1StringView(objectName));
    return label ? label->text() : QString();
}

} // namespace

TEST_F(CardStatusPageTest, OnlyTheStatesWithNothingToRenderTakeTheStatusPage)
{
    // The state-model half of the decision, driven through the SAME resolver
    // the window calls (resolveCardState) rather than a test-local restating
    // of it. Only the two states that produce no card page at all may divert
    // to the status page; every readable state must still take the ordinary
    // spinner-then-plugin path.
    using librecelik::agent::hasNoCardSurface;
    using LibreSCRS::AgentClient::PreReadAuth;
    using LibreSCRS::AgentClient::resolveCardState;
    using LibreSCRS::AgentClient::UiState;
    namespace Cap = LibreSCRS::AgentClient::Cap;

    // No plugin matched: an empty capability set resolves to UnknownCard.
    EXPECT_TRUE(hasNoCardSurface(resolveCardState(0U, PreReadAuth::None, true, false)));
    // Ancillary-only (eMRTD crypto / PIN management, no IdentityData, no PKI):
    // a driver matched but it creates no user surface.
    EXPECT_TRUE(
        hasNoCardSurface(resolveCardState(Cap::EmrtdCrypto | Cap::PinManagement, PreReadAuth::None, true, false)));

    // The pin: every readable card keeps the page flow it has today.
    EXPECT_FALSE(hasNoCardSurface(resolveCardState(Cap::IdentityData | Cap::Pki, PreReadAuth::None, true, false)));
    EXPECT_FALSE(hasNoCardSurface(resolveCardState(Cap::IdentityData, PreReadAuth::None, true, false)));
    EXPECT_FALSE(hasNoCardSurface(resolveCardState(Cap::Pki, PreReadAuth::None, true, false)));
    EXPECT_FALSE(hasNoCardSurface(resolveCardState(Cap::IdentityData, PreReadAuth::Can, true, false)));
    EXPECT_FALSE(hasNoCardSurface(UiState::NoCard));
    EXPECT_FALSE(hasNoCardSurface(UiState::None));
}

TEST_F(CardStatusPageTest, AnUnrecognisedCardGetsAPageNamingItsReaderAndItsAtr)
{
    // The dual-interface mis-tap: the contactless side of a card whose driver
    // only binds over contact matches nothing, and the holder has to be told
    // WHICH reader holds the card that could not be read — the reader
    // selector is hidden whenever there is only one page, so the page itself
    // is the only place that fact can appear.
    using LibreSCRS::AgentClient::UiState;
    librecelik::agent::CardStatusPage page(UiState::UnknownCard, QStringLiteral("ACS ACR1252 CL slot"),
                                           QStringLiteral("3B818001808012"), nullptr);

    EXPECT_TRUE(labelText(page, "cardStatusReader").contains(QStringLiteral("ACS ACR1252 CL slot")))
        << "reader line: " << labelText(page, "cardStatusReader").toStdString();
    EXPECT_TRUE(labelText(page, "cardStatusTitle").contains(QStringLiteral("3B 81 80 01 80 80")))
        << "title: " << labelText(page, "cardStatusTitle").toStdString();
    EXPECT_FALSE(labelText(page, "cardStatusHint").isEmpty()) << "the page must offer the holder something to try";
}

TEST_F(CardStatusPageTest, AnUnrecognisedCardWithoutAnAtrFallsBackToTheNoAtrSentence)
{
    // A session can surface UnknownCard before any ATR reaches the client.
    // With nothing to quote, the page must use the same sentence as the
    // no-verdict states — not the with-ATR sentence with a hole where the
    // bytes belong.
    using LibreSCRS::AgentClient::UiState;
    librecelik::agent::CardStatusPage withAtr(UiState::UnknownCard, QStringLiteral("reader"),
                                              QStringLiteral("3B818001808012"), nullptr);
    librecelik::agent::CardStatusPage withoutAtr(UiState::UnknownCard, QStringLiteral("reader"), QString(), nullptr);
    librecelik::agent::CardStatusPage errorPage(UiState::Error, QStringLiteral("reader"), QString(), nullptr);

    EXPECT_NE(labelText(withoutAtr, "cardStatusTitle"), labelText(withAtr, "cardStatusTitle"))
        << "an absent ATR must not render the with-ATR sentence";
    EXPECT_EQ(labelText(withoutAtr, "cardStatusTitle"), labelText(errorPage, "cardStatusTitle"))
        << "with nothing to quote the UnknownCard title is the plain no-verdict sentence";
    EXPECT_FALSE(labelText(withoutAtr, "cardStatusTitle").contains(QStringLiteral("  ")))
        << "no double space where the snippet would have been";
}

TEST_F(CardStatusPageTest, ACardWithNoUserSurfaceGetsTheSamePerReaderPage)
{
    // Error, not UnknownCard: a driver DID match, so there is no ATR verdict
    // to report — but the card is just as unreadable and the holder still has
    // to be told which reader it is in.
    using LibreSCRS::AgentClient::UiState;
    librecelik::agent::CardStatusPage page(UiState::Error, QStringLiteral("Gemalto PC Twin Reader"),
                                           QStringLiteral("3B9F958131FE9F00"), nullptr);

    EXPECT_TRUE(labelText(page, "cardStatusReader").contains(QStringLiteral("Gemalto PC Twin Reader")))
        << "reader line: " << labelText(page, "cardStatusReader").toStdString();
    EXPECT_FALSE(labelText(page, "cardStatusTitle").isEmpty());
    EXPECT_FALSE(labelText(page, "cardStatusHint").isEmpty());

    // The two verdicts are not the same sentence: one says no driver matched,
    // the other says the matched driver has nothing to show.
    librecelik::agent::CardStatusPage unknown(UiState::UnknownCard, QStringLiteral("Gemalto PC Twin Reader"),
                                              QStringLiteral("3B9F958131FE9F00"), nullptr);
    EXPECT_NE(labelText(page, "cardStatusTitle"), labelText(unknown, "cardStatusTitle"));
}

TEST_F(CardStatusPageTest, ALanguageChangeRebuildsThePageFromItsOwnInputs)
{
    // The page keeps the state, the reader name and the ATR, not the rendered
    // sentences: a language change re-renders through the catalog, and a page
    // that had only kept its text would either freeze in the old language or
    // lose the two substituted facts entirely.
    using LibreSCRS::AgentClient::UiState;
    librecelik::agent::CardStatusPage page(UiState::UnknownCard, QStringLiteral("Reader 7"),
                                           QStringLiteral("3B818001808012"), nullptr);
    QEvent languageChange(QEvent::LanguageChange);
    QApplication::sendEvent(&page, &languageChange);

    EXPECT_TRUE(labelText(page, "cardStatusReader").contains(QStringLiteral("Reader 7")));
    EXPECT_TRUE(labelText(page, "cardStatusTitle").contains(QStringLiteral("3B 81 80 01 80 80")));
}

TEST_F(CardStatusPageTest, TheAtrIsShownAsLeadingBytesWithTheRestMarked)
{
    // The agent hands the ATR over as a flat hex string; the display form is a
    // regrouping of characters rather than a formatting of bytes.
    using librecelik::agent::atrSnippet;
    EXPECT_EQ(atrSnippet(QStringLiteral("3b818001808012")), QStringLiteral("3B 81 80 01 80 80 ..."));
    EXPECT_EQ(atrSnippet(QStringLiteral("3B8180")), QStringLiteral("3B 81 80"));
    EXPECT_EQ(atrSnippet(QString()), QString());
    // An odd-length input (which the client contract does not produce) keeps
    // its trailing nibble rather than silently dropping it.
    EXPECT_EQ(atrSnippet(QStringLiteral("3B818")), QStringLiteral("3B 81 8"));
}
