// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

/// @file
/// @brief Self-test for the scripted fake agent backend.
///
/// The fakes are what every offscreen GUI suite in this campaign is driven by,
/// so they get their own coverage: the scripted replies, the gateway's
/// presence/config/roster recorders, the coverage of every `AgentGateway` pure
/// virtual, and — the load-bearing part — BOTH fakes asserted against BOTH
/// shared emission-contract helpers in `fake_gateway/controllercontract.h`.
/// The live controllers are asserted against those same two helpers, so fake
/// and live are pinned to ONE emission contract on both seams.

#include "fake_gateway/controllercontract.h"
#include "fake_gateway/fakeagentgateway.h"
#include "fake_gateway/fakecardcontroller.h"
#include "fake_gateway/fakesigncontroller.h"

#include "anonymous_fd.h"
#include "qstring_printto.h"

#include <LibreSCRS/AgentClient/CredentialTypes.h>
#include <LibreSCRS/AgentClient/SignOptions.h>
#include <LibreSCRS/AgentClient/SyncError.h>
#include <LibreSCRS/AgentClient/Types.h>

#include <QByteArray>
#include <QList>
#include <QSignalSpy>
#include <QVariant>

#include <gtest/gtest.h>

#include <unistd.h>

#include <cstddef>

using librecelik::test::agent::FakeAgentGateway;
using librecelik::test::agent::FakeCardController;
using librecelik::test::agent::FakeSignController;
using librecelik::test::agent::ReadContractExpectation;
using librecelik::test::agent::SignContractExpectation;

namespace {

LibreSCRS::AgentClient::FieldGroup groupWithKey(const QString& key)
{
    LibreSCRS::AgentClient::FieldGroup group;
    group.key = key;
    return group;
}

librecelik::agent::SignRowResult okRow(const QString& path)
{
    librecelik::agent::SignRowResult row;
    row.filePath = path;
    row.outputPath = path + QStringLiteral(".signed");
    row.ok = true;
    return row;
}

librecelik::agent::SignRowResult failRow(const QString& path, const QString& message)
{
    librecelik::agent::SignRowResult row;
    row.filePath = path;
    row.ok = false;
    row.message = message;
    return row;
}

librecelik::agent::SignRequestItem item(const QString& path)
{
    librecelik::agent::SignRequestItem entry;
    entry.filePath = path;
    return entry;
}

} // namespace

TEST(FakeGateway, ScriptedReadStreamsGroupsThenFinishes)
{
    FakeCardController c;
    c.scriptedCardType = QStringLiteral("rs-eid");
    LibreSCRS::AgentClient::FieldGroup g;
    g.key = QStringLiteral("personal");
    c.scriptedGroups = {g};
    QStringList seen;
    QObject::connect(&c, &librecelik::agent::CardController::groupReady, [&](const auto& grp) { seen << grp.key; });
    bool finished = false;
    QObject::connect(&c, &librecelik::agent::CardController::readingFinished, [&] { finished = true; });
    c.startRead();
    EXPECT_EQ(seen, QStringList{QStringLiteral("personal")});
    EXPECT_TRUE(finished);
}

TEST(FakeGateway, PresenceFlipsEmitSignal)
{
    FakeAgentGateway gw;
    QSignalSpy spy(&gw, &librecelik::agent::AgentGateway::presenceChanged);
    gw.setPresence(librecelik::agent::PresenceState::Ready);
    ASSERT_EQ(spy.count(), 1);
}

TEST(FakeGateway, ScriptedReadSatisfiesTheSharedEmissionContract)
{
    FakeCardController c;
    c.scriptedGroups = {groupWithKey(QStringLiteral("personal")), groupWithKey(QStringLiteral("address"))};
    ReadContractExpectation expectation;
    expectation.expectedGroupKeys = QStringList{QStringLiteral("personal"), QStringLiteral("address")};
    librecelik::test::agent::assertReadEmissionContract(c, expectation);
}

TEST(FakeGateway, ScriptedPhotoReadPutsThePhotoGroupLastAndInTheModel)
{
    FakeCardController c;
    c.scriptedGroups = {groupWithKey(QStringLiteral("personal"))};
    c.scriptedPhotoGroup = groupWithKey(QStringLiteral("photo"));
    ReadContractExpectation expectation;
    expectation.expectPhotoGroup = true;
    expectation.expectedGroupKeys = QStringList{QStringLiteral("personal"), QStringLiteral("photo")};
    librecelik::test::agent::assertReadEmissionContract(c, expectation);
}

TEST(FakeGateway, FailedReadSurfacesTheErrorAndNeverAnIdentityModel)
{
    // The failed-read half of the shared contract. A fake that announced an
    // identity model for a read that failed would let a display suite pass on
    // a sequence the live controller never produces, so the ordering is pinned
    // here rather than left to the one GUI case that only reads the message.
    FakeCardController c;
    c.scriptedGroups = {groupWithKey(QStringLiteral("personal"))};
    c.scriptedError = QStringLiteral("the card was removed");
    c.failNextRead = true;
    ReadContractExpectation expectation;
    expectation.expectError = true;
    librecelik::test::agent::assertReadEmissionContract(c, expectation);
}

TEST(FakeGateway, ScriptedSignRunSatisfiesTheSharedEmissionContract)
{
    FakeSignController s;
    s.scriptedPhases = {LibreSCRS::AgentClient::OperationPhase::AwaitingConsent};
    s.scriptedRows = {okRow(QStringLiteral("/tmp/a.pdf")),
                      failRow(QStringLiteral("/tmp/b.pdf"), QStringLiteral("nope"))};
    SignContractExpectation expectation;
    expectation.expectedTally = qMakePair(1, 1);
    librecelik::test::agent::assertSignEmissionContract(
        s, QStringLiteral("cert-1"), {item(QStringLiteral("/tmp/a.pdf")), item(QStringLiteral("/tmp/b.pdf"))},
        LibreSCRS::AgentClient::SignOptions{}, QStringLiteral("/tmp/out"), expectation);
    EXPECT_EQ(s.lastCertId, QStringLiteral("cert-1"));
    EXPECT_EQ(s.lastOutputFolder, QStringLiteral("/tmp/out"));
}

TEST(FakeGateway, HeldOpenSignRunEmitsNothingUntilReleasedAndRecordsCancel)
{
    FakeSignController s;
    s.holdOpen = true;
    s.scriptedRows = {okRow(QStringLiteral("/tmp/a.pdf"))};
    QSignalSpy finishedSpy(&s, &librecelik::agent::SignController::finished);
    s.start(QStringLiteral("cert-1"), {item(QStringLiteral("/tmp/a.pdf"))}, LibreSCRS::AgentClient::SignOptions{},
            QStringLiteral("/tmp/out"));
    EXPECT_EQ(finishedSpy.count(), 0);
    s.cancel();
    EXPECT_TRUE(s.cancelCalled);
    s.releaseHold();
    ASSERT_EQ(finishedSpy.count(), 1);
    EXPECT_EQ(finishedSpy.takeFirst(), (QList<QVariant>{1, 0}));
}

TEST(FakeGateway, EveryCardVerbIsLoggedAndRepliesWithItsScript)
{
    FakeCardController c;
    c.scriptedCerts = {LibreSCRS::AgentClient::CertificateInfo{}};
    c.scriptedTokenInfo = groupWithKey(QStringLiteral("token"));
    c.scriptedCredentials = {LibreSCRS::AgentClient::CredentialRecord{}};
    c.scriptedPinResult.outcome = LibreSCRS::AgentClient::CredentialOutcome::Ok;

    int certs = 0;
    int tokens = 0;
    int creds = 0;
    int pins = 0;
    QObject::connect(&c, &librecelik::agent::CardController::certificatesReady,
                     [&](const auto& list) { certs = static_cast<int>(list.size()); });
    QObject::connect(&c, &librecelik::agent::CardController::tokenInfoReady,
                     [&](const auto& group) { tokens = group.key == QStringLiteral("token") ? 1 : 0; });
    QObject::connect(&c, &librecelik::agent::CardController::credentialsReady,
                     [&](const auto& list) { creds = static_cast<int>(list.size()); });
    QObject::connect(&c, &librecelik::agent::CardController::pinResultReady, [&](const auto&) { ++pins; });

    c.requestCertificates();
    c.requestTokenInfo();
    c.requestCredentials();
    c.managePin(QStringLiteral("pin-1"), LibreSCRS::AgentClient::PinVerb::Change);
    c.activateSigningKey();
    c.cancel();

    EXPECT_EQ(certs, 1);
    EXPECT_EQ(tokens, 1);
    EXPECT_EQ(creds, 1);
    EXPECT_EQ(pins, 2);
    EXPECT_EQ(c.callLog, (QStringList{QStringLiteral("requestCertificates"), QStringLiteral("requestTokenInfo"),
                                      QStringLiteral("requestCredentials"), QStringLiteral("managePin:pin-1"),
                                      QStringLiteral("activateSigningKey"), QStringLiteral("cancel")}));
}

TEST(FakeGateway, RegisteredControllersAreReturnedPerCardIdAndUnknownIdsAreNull)
{
    FakeAgentGateway gw;
    FakeCardController card;
    FakeSignController sign;
    gw.registerCardController(QStringLiteral("card-1"), &card);
    gw.registerSignController(QStringLiteral("card-1"), &sign);
    EXPECT_EQ(gw.cardController(QStringLiteral("card-1")), &card);
    EXPECT_EQ(gw.signController(QStringLiteral("card-1")), &sign);
    EXPECT_EQ(gw.cardController(QStringLiteral("nope")), nullptr);
    EXPECT_EQ(gw.signController(QStringLiteral("nope")), nullptr);
}

TEST(FakeGateway, RemoveCardClearsTheRosterEntryAndAnnouncesIt)
{
    FakeAgentGateway gw;
    librecelik::agent::ReaderInfo reader;
    reader.id = QStringLiteral("reader-1");
    reader.hasCard = true;
    reader.cardId = QStringLiteral("card-1");
    gw.scriptedReaders = {reader};
    QSignalSpy removedSpy(&gw, &librecelik::agent::AgentGateway::cardRemoved);
    gw.removeCard(QStringLiteral("card-1"));
    ASSERT_EQ(removedSpy.count(), 1);
    EXPECT_EQ(removedSpy.takeFirst().at(0).toString(), QStringLiteral("card-1"));
    ASSERT_EQ(gw.readers().size(), 1);
    EXPECT_FALSE(gw.readers().at(0).hasCard);
    EXPECT_TRUE(gw.readers().at(0).cardId.isEmpty());
}

TEST(FakeGateway, ConfigWritesAndResetsAreRecordedAndAnnounced)
{
    FakeAgentGateway gw;
    QSignalSpy changedSpy(&gw, &librecelik::agent::AgentGateway::configChanged);
    EXPECT_FALSE(gw.setConfigValue(QStringLiteral("DefaultLevel"), QStringLiteral("b-t")).has_value());
    EXPECT_EQ(gw.configSnapshot().value(QStringLiteral("DefaultLevel")).toString(), QStringLiteral("b-t"));
    EXPECT_FALSE(gw.resetConfigValue(QStringLiteral("DefaultLevel")).has_value());
    EXPECT_FALSE(gw.configSnapshot().contains(QStringLiteral("DefaultLevel")));
    EXPECT_EQ(gw.configWrites.size(), 1);
    EXPECT_EQ(gw.configWrites.last(), qMakePair(QStringLiteral("DefaultLevel"), QVariant(QStringLiteral("b-t"))));
    EXPECT_EQ(gw.configResets, QStringList{QStringLiteral("DefaultLevel")});
    EXPECT_EQ(changedSpy.count(), 2);
}

TEST(FakeGateway, ScriptedRefusalStopsTheWriteButStillRecordsTheAttempt)
{
    FakeAgentGateway gw;
    gw.nextRefusal = LibreSCRS::AgentClient::SyncError::NotAuthorized;
    QSignalSpy changedSpy(&gw, &librecelik::agent::AgentGateway::configChanged);
    const auto writeError = gw.setConfigValue(QStringLiteral("TslUrls"), QStringLiteral("https://x/tl.xml"));
    ASSERT_TRUE(writeError.has_value());
    EXPECT_EQ(*writeError, LibreSCRS::AgentClient::SyncError::NotAuthorized);
    const auto resetError = gw.resetConfigValue(QStringLiteral("TslUrls"));
    ASSERT_TRUE(resetError.has_value());
    EXPECT_EQ(*resetError, LibreSCRS::AgentClient::SyncError::NotAuthorized);
    EXPECT_EQ(gw.configWrites.size(), 1);
    EXPECT_EQ(gw.configResets.size(), 1);
    EXPECT_FALSE(gw.configSnapshot().contains(QStringLiteral("TslUrls")));
    EXPECT_EQ(changedSpy.count(), 0);
}

TEST(FakeGateway, ImportConsumesTheDescriptorItWasHandedAndAnswersTheScript)
{
    // The fake models the ONE thing a value-carrying seam cannot: the receiver
    // reads the descriptor from its CURRENT position, so the sender's own file
    // position moves. A fake that read by path, or pread(2) at offset 0, would
    // let a dialog that passed a name look identical to one that passed a
    // descriptor -- which is the whole distinction the dialog suite asserts.
    FakeAgentGateway gw;
    gw.scriptedAnchorState.anchors = 5;
    gw.scriptedAnchorState.issuers = 2;

    const int fd = librecelik::test::makeAnonymousFd("fake-master-list");
    ASSERT_GE(fd, 0);
    const QByteArray bytes = QByteArrayLiteral("MASTER-LIST");
    ASSERT_EQ(::write(fd, bytes.constData(), static_cast<std::size_t>(bytes.size())), bytes.size());
    ASSERT_EQ(::lseek(fd, 0, SEEK_SET), 0);

    const auto installed = gw.importCscaMasterList(fd);
    ASSERT_TRUE(installed.has_value());
    EXPECT_EQ(installed->anchors, 5u);
    EXPECT_EQ(installed->issuers, 2u);
    EXPECT_EQ(gw.importedFds, QList<int>{fd});
    EXPECT_EQ(gw.importedBytes, QList<QByteArray>{bytes});
    EXPECT_EQ(::lseek(fd, 0, SEEK_CUR), bytes.size()) << "the fake's read must move the sender's offset";

    gw.nextImportRefusal = LibreSCRS::AgentClient::SyncError::MasterListReplayed;
    const auto refused = gw.importCscaMasterList(fd);
    ASSERT_FALSE(refused.has_value());
    EXPECT_EQ(refused.error(), LibreSCRS::AgentClient::SyncError::MasterListReplayed);
    EXPECT_EQ(gw.importedFds.size(), 2) << "a refusal is still an attempt, and the record needs it";
    ::close(fd);
}

TEST(FakeGateway, PresenceRosterFeatureAndVersionAccessorsAnswerTheScript)
{
    FakeAgentGateway gw;
    EXPECT_EQ(gw.presence(), librecelik::agent::PresenceState::AgentMissing);
    gw.scriptedVersion = QStringLiteral("4.3.0");
    gw.scriptedFeatures = {QStringLiteral("batch-sign")};
    EXPECT_EQ(gw.agentVersion(), QStringLiteral("4.3.0"));
    EXPECT_TRUE(gw.hasFeature(QStringLiteral("batch-sign")));
    EXPECT_FALSE(gw.hasFeature(QStringLiteral("visual-sign")));
    gw.refresh();
    EXPECT_EQ(gw.callLog, QStringList{QStringLiteral("refresh")});
}

TEST(FakeGateway, LayoutAndFontDefaultToTheFeatureAbsentShape)
{
    FakeAgentGateway gw;
    EXPECT_FALSE(gw.layoutVisualSignature(QStringLiteral("text"), QRectF(0, 0, 100, 50)).has_value());
    EXPECT_TRUE(gw.appearanceFontData().isEmpty());

    LibreSCRS::AgentClient::LayoutResult layout;
    layout.fontSize = 9.0;
    layout.lines = QStringList{QStringLiteral("line")};
    gw.scriptedLayout = layout;
    gw.scriptedFontData = QByteArrayLiteral("font-bytes");
    const auto resolved = gw.layoutVisualSignature(QStringLiteral("text"), QRectF(0, 0, 100, 50));
    ASSERT_TRUE(resolved.has_value());
    EXPECT_DOUBLE_EQ(resolved->fontSize, 9.0);
    EXPECT_EQ(gw.appearanceFontData(), QByteArrayLiteral("font-bytes"));
}

TEST(FakeGateway, CertificateDerAnswersTheScriptAndEmptyBytesForAnUnknownId)
{
    FakeAgentGateway gw;
    gw.scriptedDer.insert(QStringLiteral("cert-1"), QByteArrayLiteral("\x30\x82"));
    QSignalSpy derSpy(&gw, &librecelik::agent::AgentGateway::certificateDerReady);

    gw.fetchCertificateDer(QStringLiteral("reader-1"), QStringLiteral("cert-1"));
    ASSERT_EQ(derSpy.count(), 1);
    EXPECT_EQ(derSpy.takeFirst().at(1).toByteArray(), QByteArrayLiteral("\x30\x82"));

    gw.fetchCertificateDer(QStringLiteral("reader-1"), QStringLiteral("missing"));
    ASSERT_EQ(derSpy.count(), 1);
    EXPECT_TRUE(derSpy.takeFirst().at(1).toByteArray().isEmpty());

    EXPECT_EQ(gw.callLog, (QStringList{QStringLiteral("fetchCertificateDer:cert-1"),
                                       QStringLiteral("fetchCertificateDer:missing")}));
}
