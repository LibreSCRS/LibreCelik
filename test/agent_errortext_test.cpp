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
