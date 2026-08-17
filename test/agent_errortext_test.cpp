// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#include "agent/errortext.h"

#include <QString>

#include <gtest/gtest.h>

using namespace LibreSCRS::AgentClient;

TEST(ErrorText, CallErrorWinsWhenEngaged)
{
    const QString t = librecelik::agent::errorText(ErrorCode::None, CallError::AgentUnavailable, {}, {});
    EXPECT_FALSE(t.isEmpty());
    EXPECT_NE(t, librecelik::agent::errorText(ErrorCode::CardRemoved, CallError::None, {}, {}));
}
TEST(ErrorText, UnknownMsgKeyFallsBackToAgentFallbackText)
{
    EXPECT_EQ(librecelik::agent::errorText(ErrorCode::ParseError, CallError::None, QStringLiteral("future.key"),
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
