// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/errortext.h"

#include <QCoreApplication>
#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include <algorithm>
#include <ostream>

using namespace LibreSCRS::AgentClient;

// Readable QString diagnostics. Without it a copy mismatch renders as pages of
// raw UTF-16 byte objects, which is precisely useless for the assertions below —
// their whole subject is which words a user ends up reading.
inline void PrintTo(const QString& value, std::ostream* os)
{
    *os << '"' << value.toStdString() << '"';
}

TEST(ErrorText, CallErrorWinsWhenEngaged)
{
    const QString t = librecelik::agent::errorText(ErrorCode::None, CallError::AgentUnavailable, {}, {});
    EXPECT_FALSE(t.isEmpty());
    EXPECT_NE(t, librecelik::agent::errorText(ErrorCode::CardRemoved, CallError::None, {}, {}));
}

// An unknown key no longer buys the agent's prose a free pass: the code axis is
// consulted first, and a code this build names answers in the user's language.
TEST(ErrorText, NamedCodeBeatsTheAgentsProseEvenWithAKeyPresent)
{
    const QString prose = QStringLiteral("Future thing");
    const QString text =
        librecelik::agent::errorText(ErrorCode::ParseError, CallError::None, QStringLiteral("future.key"), prose);
    EXPECT_NE(text, prose);
    EXPECT_FALSE(text.isEmpty());
    // It is the copy for THIS code, not a generic that happens not to be prose.
    EXPECT_NE(text, librecelik::agent::errorText(ErrorCode::None, CallError::None, {}, {}));
}

// The other half of the same rule: when neither axis names the failure, the
// agent's authored message is all that carries meaning and must still reach the
// user untouched. Both an unnamed code and one past this build's tail.
TEST(ErrorText, UnnamedCodeStillPassesTheAgentsFallbackText)
{
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, CallError::None, QStringLiteral("future.key"),
                                           QStringLiteral("Future thing")),
              QStringLiteral("Future thing"));
    EXPECT_EQ(librecelik::agent::errorText(static_cast<ErrorCode>(9999), CallError::None, QStringLiteral("future.key"),
                                           QStringLiteral("Future thing")),
              QStringLiteral("Future thing"));
}
TEST(ErrorText, AnsweredRefusalPassesTheAgentsProseThroughWithoutAKey)
{
    // The entry-refusal shape (AgentOperation::failEntry): the agent ANSWERED
    // with a classified refusal — errorCode None, callError InvalidArguments /
    // AccessDenied / ProtocolError — and its authored message arrives as the
    // FALLBACK with an EMPTY key. That prose is the only precise record of
    // why (the Leg-1 bench catch: "tsaUrl is only meaningful for the
    // timestamped/long-term family" rendered as the generic no-reason line).
    const QString prose = QStringLiteral("tsaUrl is only meaningful for the timestamped/long-term family");
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, CallError::InvalidArguments, {}, prose), prose);
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, CallError::AccessDenied, {}, prose), prose);
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, CallError::ProtocolError, {}, prose), prose);
}

TEST(ErrorText, TransportClassFailuresKeepTheCoarseCopyOverRawBusText)
{
    // The transport axis never reached (or never heard back from) the agent:
    // its message is raw bus text written for a developer, and the coarse
    // localized line must win.
    const QString busText = QStringLiteral("org.freedesktop.DBus.Error.NoReply: did not receive a reply");
    EXPECT_NE(librecelik::agent::errorText(ErrorCode::None, CallError::Timeout, {}, busText), busText);
    EXPECT_NE(librecelik::agent::errorText(ErrorCode::None, CallError::AgentUnavailable, {}, busText), busText);
    EXPECT_NE(librecelik::agent::errorText(ErrorCode::None, CallError::TransportFailure, {}, busText), busText);
}

TEST(ErrorText, UnknownFutureCodeYieldsGenericNotEmpty)
{
    EXPECT_FALSE(librecelik::agent::errorText(static_cast<ErrorCode>(9999), CallError::None, {}, {}).isEmpty());
}
TEST(PhaseText, EveryEnumeratorIncludingCreatedHasText)
{
    // Created (=0) is a REAL pre-first-report value, not a sentinel; a
    // future unknown value falls to the same created/preparing text.
    for (auto p : {OperationPhase::Created, OperationPhase::Connecting, OperationPhase::AwaitingConsent,
                   OperationPhase::Authenticating, OperationPhase::Reading, OperationPhase::Signing,
                   OperationPhase::Timestamping, OperationPhase::Done})
        EXPECT_FALSE(librecelik::agent::phaseText(p).isEmpty());
    EXPECT_FALSE(librecelik::agent::phaseText(static_cast<OperationPhase>(99)).isEmpty());
}
TEST(OutcomeText, UnspecifiedRendersItsOwnText)
{
    EXPECT_FALSE(librecelik::agent::outcomeText(CredentialOutcome::Unspecified).isEmpty());
}

namespace {

/// Installs the Serbian catalog for the duration of one test, because the
/// defect this suite pins is invisible in English: the leaked string and the
/// correct one are the same words when the session language is the one the
/// agent authors its fallbacks in. The translator is removed again in TearDown
/// so it cannot colour any other suite in this binary.
///
/// The .qm directory is baked in at compile time (see this target's
/// LIBRECELIK_TRANSLATIONS_DIR_DEFAULT in test/CMakeLists.txt) — the same
/// pattern SlotLabelFormatterTests uses, and it avoids depending on a CTest
/// ENVIRONMENT property.
class ErrorTextSerbian : public ::testing::Test
{
protected:
    void SetUp() override
    {
        ASSERT_NE(QCoreApplication::instance(), nullptr);
        translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_sr_RS"), qmDir))
            << "failed to load LibreCelik_sr_RS.qm from " << qmDir.toStdString();
        ASSERT_TRUE(QCoreApplication::installTranslator(translator));
    }

    void TearDown() override
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    /// Serbian ships in Cyrillic, so script is a stable proof that the catalog
    /// answered — stronger than comparing against a sentence copied out of the
    /// .ts file, which would only re-assert the copy against itself.
    [[nodiscard]] static bool hasCyrillic(const QString& text)
    {
        return std::ranges::any_of(text, [](QChar c) { return c.script() == QChar::Script_Cyrillic; });
    }

    QTranslator* translator = nullptr;
};

} // namespace

// THE regression: a failure the agent answered with a code AND an English
// sentence must render in the session language. Before the resolution order was
// inverted, the presence of any message key handed the English sentence
// straight to a Serbian-speaking user.
TEST_F(ErrorTextSerbian, NamedCodeRendersSerbianRatherThanTheAgentsEnglishProse)
{
    const QString prose = QStringLiteral("The data read from the card could not be interpreted.");
    const QString text =
        librecelik::agent::errorText(ErrorCode::ParseError, CallError::None, QStringLiteral("future.key"), prose);

    EXPECT_NE(text, prose) << "the agent's English prose reached a Serbian session";
    EXPECT_TRUE(hasCyrillic(text)) << "rendered \"" << text.toStdString() << "\", which is not the Serbian copy";
}

// A cancelled operation reports NO error code, so the code axis declines and
// nothing but the key can answer for it. Until the key was named, that left the
// agent's own bare "Operation cancelled" — the one failure where an English line
// reached the user with nothing lost by translating it.
TEST_F(ErrorTextSerbian, CancellationRendersSerbianRatherThanTheAgentsBareEnglish)
{
    const QString prose = QStringLiteral("Operation cancelled");
    const QString text =
        librecelik::agent::errorText(ErrorCode::None, CallError::None, QStringLiteral("op.cancelled"), prose);

    EXPECT_NE(text, prose);
    EXPECT_TRUE(hasCyrillic(text)) << "rendered \"" << text.toStdString() << "\", which is not the Serbian copy";
    // Not the generic either: a cancellation is a specific thing to say.
    EXPECT_NE(text, librecelik::agent::errorText(ErrorCode::None, CallError::None, QString(), QString()));
}

// The same for the transport axis, which had the ordering right already — pinned
// here so both axes are proven under a loaded catalog rather than one of them
// being taken on trust.
TEST_F(ErrorTextSerbian, TransportClassRendersSerbianRatherThanRawBusText)
{
    const QString busText = QStringLiteral("org.freedesktop.DBus.Error.NoReply: did not receive a reply");
    const QString text = librecelik::agent::errorText(ErrorCode::None, CallError::Timeout, {}, busText);

    EXPECT_NE(text, busText);
    EXPECT_TRUE(hasCyrillic(text)) << "rendered \"" << text.toStdString() << "\", which is not the Serbian copy";
}

// And the deliberate exception, under the same catalog: when LC cannot name the
// failure, an English sentence about what actually went wrong is still better
// than a localized sentence about nothing. This is the trade the ordering makes,
// asserted so a later "translate everything" change cannot make it silently.
TEST_F(ErrorTextSerbian, UnnamedFailureStillShowsTheAgentsProseInASerbianSession)
{
    const QString prose = QStringLiteral("tsaUrl is only meaningful for the timestamped/long-term family");
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, CallError::InvalidArguments, {}, prose), prose);
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, CallError::None, QStringLiteral("future.key"), prose),
              prose);
}
