// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/CallError.h>
#include <LibreSCRS/AgentClient/CredentialTypes.h> // CredentialOutcome
#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <QString>

namespace librecelik::agent {

/// @brief Can repeating a card read that failed with @p code plausibly succeed?
///
/// The window drops a card's page when its read fails while still showing the
/// spinner, and refuses to re-add it until the card is physically re-seated.
/// That is right for a card this build cannot read — it stops one failure
/// becoming a retry storm — and wrong for every failure the HOLDER can clear,
/// because it removes the surface they would clear it on. The entry window
/// expiring is the sharp case: the status bar says "try again" while the page
/// that would let them is already gone.
///
/// Retryable: the holder ran out of time, dismissed the prompt, or gave a wrong
/// secret; the credential helper was momentarily unusable. Everything else — a
/// card this build does not support, data it cannot parse, a transport that
/// failed — is latched.
[[nodiscard]] bool isRetryableReadFailure(LibreSCRS::AgentClient::ErrorCode code);

/// Localized text for a failed operation. Resolution order, first match wins:
///
///   1. a msgKey this build names → LC's catalog string for it;
///   2. an ErrorCode this build names → LC's catalog string as the HEADLINE;
///      when the agent's msgFallback says something the headline does not, it
///      rides below as a detail line — on the classes where one code covers
///      many distinct failures, that prose is the only specific record of
///      which one happened, and a coarser localized sentence must not erase
///      it;
///   3. an ANSWERED refusal (`InvalidArguments` / `AccessDenied` /
///      `ProtocolError`) carrying a message → that message;
///   4. any other engaged CallError → LC's catalog string for the transport
///      classification, ahead of the raw bus text riding along with it;
///   5. a non-blank msgFallback → the agent's authored message;
///   6. otherwise → a localized generic.
///
/// LC's own copy leads on both axes it can name, and the agent's msgFallback
/// is a degraded floor rather than a preference. The agent authors that
/// fallback in English and nothing translates it on the way here, so
/// rendering it AHEAD of a string LC already holds in the user's language
/// puts English into a Serbian session — but where it carries the failure's
/// specifics, hiding it entirely would trade accuracy for language purity,
/// hence the headline-plus-detail shape.
///
/// Wire-frozen append-only: a code or key this build does not name simply
/// declines its rule and resolution continues; nothing asserts.
///
/// A blank msgFallback counts as no message throughout — it is non-empty to
/// QString and empty to a reader. The result is never empty.
[[nodiscard]] QString errorText(LibreSCRS::AgentClient::ErrorCode code, LibreSCRS::AgentClient::CallError call,
                                const QString& msgKey, const QString& msgFallback);
[[nodiscard]] QString phaseText(LibreSCRS::AgentClient::OperationPhase phase);
[[nodiscard]] QString outcomeText(LibreSCRS::AgentClient::CredentialOutcome outcome);

} // namespace librecelik::agent
