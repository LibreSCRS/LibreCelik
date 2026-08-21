// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Every string here is fetched with qtTrId() at CALL time. Nothing is cached
// in a static QString or a lookup table: a translator installed after this
// translation unit was first entered — which is exactly what a language switch
// at runtime is — must change what the next call returns.

#include "agent/errortext.h"

#include <QDebug>

#include <QCoreApplication>

#include <optional>

namespace librecelik::agent {

namespace {

using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::CredentialOutcome;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::OperationPhase;

/// LC copy for the agent's wire-frozen error taxonomy — or nothing at all when
/// this build does not name the code.
///
/// The empty answer is the point of the `std::optional`: "this build has copy
/// for that code" is a question `errorText` has to ASK before it decides whether
/// the agent's own message is worth rendering, and a function that folded the
/// unnamed case into a generic string could not be asked it. Two values land on
/// the empty answer:
///
///   * `ErrorCode::None` — a failure that named no code has nothing to say, and
///     whatever else the operation carries is more specific than a generic.
///   * anything past this build's tail. The client library decodes an ErrorCode
///     value it does not know through verbatim rather than rejecting it (see
///     ErrorCode.h's append-tolerance note), so the `default:` arm is REQUIRED,
///     not stylistic, and an unrecognised value is opaque display data.
[[nodiscard]] std::optional<QString> namedCodeText(ErrorCode code)
{
    switch (code) {
    case ErrorCode::CardRemoved:
        return qtTrId("lc-agent-error-card-removed");
    case ErrorCode::CredentialWrong:
        return qtTrId("lc-agent-error-credential-wrong");
    case ErrorCode::CredentialBlocked:
        return qtTrId("lc-agent-error-credential-blocked");
    case ErrorCode::CommunicationError:
        return qtTrId("lc-agent-error-communication");
    case ErrorCode::ParseError:
        return qtTrId("lc-agent-error-parse");
    case ErrorCode::UnsupportedCard:
        return qtTrId("lc-agent-error-unsupported-card");
    case ErrorCode::AuthFailed:
        return qtTrId("lc-agent-error-auth-failed");
    case ErrorCode::PrompterError:
        return qtTrId("lc-agent-error-prompter");
    case ErrorCode::CapabilityMissing:
        return qtTrId("lc-agent-error-capability-missing");
    case ErrorCode::WatchdogTimeout:
        return qtTrId("lc-agent-error-watchdog");
    case ErrorCode::KeyNotFound:
        return qtTrId("lc-agent-error-key-not-found");
    case ErrorCode::KeyAmbiguous:
        return qtTrId("lc-agent-error-key-ambiguous");
    case ErrorCode::CertExpiredBlocked:
        return qtTrId("lc-agent-error-cert-expired-blocked");
    case ErrorCode::ChainIncomplete:
        return qtTrId("lc-agent-error-chain-incomplete");
    case ErrorCode::TsaUnreachable:
        return qtTrId("lc-agent-error-tsa-unreachable");
    case ErrorCode::SigningEngineError:
        return qtTrId("lc-agent-error-signing-engine");
    case ErrorCode::RateLimited:
        return qtTrId("lc-agent-error-rate-limited");
    case ErrorCode::EngineUnavailable:
        return qtTrId("lc-agent-error-engine-unavailable");
    case ErrorCode::InvalidDocument:
        return qtTrId("lc-agent-error-invalid-document");
    case ErrorCode::EntryExpired:
        return qtTrId("lc-agent-error-entry-expired");
    case ErrorCode::None:
    default:
        return std::nullopt;
    }
}

/// Copy for the client-local transport axis — the failures that never reached
/// the agent, so the only other string in play is raw bus text written for a
/// developer. The vocabulary is coarser than the wire taxonomy by construction
/// (several distinct wire failures collapse onto one enumerator), so the copy
/// stays at the granularity the enumerator can actually support.
///
/// `CallError::None` is unreachable from `errorText` (it guards the call) but
/// is answered anyway, on the generic line, so this helper is total.
[[nodiscard]] QString callText(CallError call)
{
    switch (call) {
    case CallError::AgentUnavailable:
        return qtTrId("lc-agent-call-unavailable");
    case CallError::Timeout:
        return qtTrId("lc-agent-call-timeout");
    case CallError::AccessDenied:
        return qtTrId("lc-agent-call-access-denied");
    case CallError::InvalidArguments:
        return qtTrId("lc-agent-call-invalid-arguments");
    case CallError::TransportFailure:
        return qtTrId("lc-agent-call-transport");
    case CallError::ProtocolError:
        return qtTrId("lc-agent-call-protocol");
    case CallError::None:
    default:
        return qtTrId("lc-agent-error-generic");
    }
}

/// LC copy for an agent-authored message key, or an empty string when this
/// build does not name the key.
///
/// Only keys whose meaning is EXACTLY an entry LC already renders are named
/// here; a key LC would have to paraphrase is deliberately left unknown, so the
/// agent's own authored fallback is what a user reads rather than a guess made
/// on this side. The key vocabulary is the agent's and it grows independently
/// of this build, which is the whole reason the fallback pass-through exists.
///
/// Naming a key is not free, because this rule resolves AHEAD of the error code:
/// a key coarser than the code it travels with would displace a more precise
/// localized line with a vaguer one. Most of the agent's vocabulary is in
/// exactly that position — one key covering many distinct failures that each
/// arrive with their own code — and is deliberately absent here for that reason.
/// The keys that belong here are the ones whose failures report NO code, so
/// there is nothing more precise for them to displace:
///
///   * `op.card_removed` and `op.watchdog_timeout` both map onto the copy their
///     own code would have produced, so the rule order cannot change what a user
///     reads either way.
///   * `op.cancelled` reports no code at all, and the message riding with it is
///     a bare restatement of the key. Without an entry here it was the one
///     failure where an untranslated English line reached the user with nothing
///     lost by translating it.
[[nodiscard]] QString knownMsgKeyText(const QString& msgKey)
{
    if (msgKey == QLatin1StringView("op.card_removed"))
        return qtTrId("lc-agent-error-card-removed");
    if (msgKey == QLatin1StringView("op.watchdog_timeout"))
        return qtTrId("lc-agent-error-watchdog");
    if (msgKey == QLatin1StringView("op.cancelled"))
        return qtTrId("lc-agent-msg-cancelled");
    return {};
}

} // namespace

QString errorText(ErrorCode code, CallError call, const QString& msgKey, const QString& msgFallback)
{
    // A whitespace-only agent message is not a message: non-empty to QString,
    // blank to a reader. Normalised once, here, so no arm below can hand a user
    // an empty error line.
    const QString message = msgFallback.trimmed().isEmpty() ? QString() : msgFallback;

    // 1. A key this build names. The most specific thing anyone knows about the
    //    failure, in the user's own language.
    if (!msgKey.isEmpty()) {
        if (const QString named = knownMsgKeyText(msgKey); !named.isEmpty())
            return named;
    }
    // 2. A code this build names. The agent's message for such a failure is
    //    authored in English and is never translated on the way here, so
    //    preferring it — which is what this function used to do for every
    //    failure that carried a key — leaked English into every non-English
    //    session on the whole agent-answered class of errors. LC's copy for a
    //    code it recognises is the localized HEADLINE — but it is coarser than
    //    the prose on the classes where the agent collapses distinct failures
    //    onto one code and carries the distinction only in its message (a
    //    signing-engine error names the actual engine complaint there; a
    //    reader-communication failure says WHICH leg failed). Discarding that
    //    prose would trade the only specific record of the failure for a
    //    general sentence, so it rides below the headline as a detail line,
    //    and is logged either way so a report without a screenshot still
    //    carries it.
    if (const std::optional<QString> named = namedCodeText(code)) {
        if (!message.isEmpty() && message != *named) {
            // Plain qWarning, not a category: this TU is compiled into test
            // targets that do not carry the logging TU, and the line must not
            // cost them a link dependency.
            qWarning() << "agent detail behind localized error" << static_cast<quint32>(code) << ":" << message;
            return *named + QLatin1Char('\n') + message;
        }
        return *named;
    }
    // 3. An ANSWERED refusal (arguments/authorization/protocol): the agent
    //    classified the request and authored the message — arriving with an
    //    empty key on the entry-refusal path (AgentOperation::failEntry) — and
    //    that prose is the only precise record of why. Rule 2 cannot have
    //    consumed this shape: an entry refusal reports no error code.
    const bool agentAnswered =
        call == CallError::InvalidArguments || call == CallError::AccessDenied || call == CallError::ProtocolError;
    if (agentAnswered && !message.isEmpty())
        return message;
    // 4. The transport axis, ahead of the agent's prose: on these routes the
    //    message is raw bus text written for a developer, never shown to a user.
    //    The two classification axes are mutually exclusive on a failed op —
    //    when the call never reached the agent, `code` is None and carries
    //    nothing.
    if (call != CallError::None)
        return callText(call);
    // 5. The agent's authored message, in the degraded position it belongs in:
    //    reachable only once neither axis named the failure, which is when that
    //    message is the only thing left that carries meaning. Untranslated, and
    //    that is the honest trade — an English sentence about the actual failure
    //    beats a localized sentence about nothing.
    if (!message.isEmpty())
        return message;
    // 6. The floor that makes a blank error line impossible.
    return qtTrId("lc-agent-error-generic");
}

QString phaseText(OperationPhase phase)
{
    switch (phase) {
    case OperationPhase::Connecting:
        return qtTrId("lc-agent-phase-connecting");
    case OperationPhase::AwaitingConsent:
        return qtTrId("lc-agent-phase-awaiting-consent");
    case OperationPhase::Authenticating:
        return qtTrId("lc-agent-phase-authenticating");
    case OperationPhase::Reading:
        return qtTrId("lc-agent-phase-reading");
    case OperationPhase::Signing:
        return qtTrId("lc-agent-phase-signing");
    case OperationPhase::Timestamping:
        return qtTrId("lc-agent-phase-timestamping");
    case OperationPhase::Done:
        return qtTrId("lc-agent-phase-done");
    case OperationPhase::Created:
    default:
        // Created is a REAL value, not a sentinel: an operation reports it
        // before its first progress notification arrives. A phase appended by a
        // newer agent shares the line because "preparing" is the honest thing to
        // say about a step this build cannot name — never a blank progress label.
        return qtTrId("lc-agent-phase-created");
    }
}

QString outcomeText(CredentialOutcome outcome)
{
    switch (outcome) {
    case CredentialOutcome::Ok:
        return qtTrId("lc-agent-outcome-ok");
    case CredentialOutcome::UserCancelled:
        return qtTrId("lc-agent-outcome-cancelled");
    case CredentialOutcome::MissingFields:
        return qtTrId("lc-agent-outcome-missing-fields");
    case CredentialOutcome::InvalidPin:
        return qtTrId("lc-agent-outcome-invalid-pin");
    case CredentialOutcome::Blocked:
        return qtTrId("lc-agent-outcome-blocked");
    case CredentialOutcome::PluginError:
        return qtTrId("lc-agent-outcome-plugin-error");
    case CredentialOutcome::Unsupported:
        return qtTrId("lc-agent-outcome-unsupported");
    case CredentialOutcome::KeyActivationFailed:
        return qtTrId("lc-agent-outcome-key-activation-failed");
    case CredentialOutcome::CardRemoved:
        return qtTrId("lc-agent-outcome-card-removed");
    case CredentialOutcome::EntryExpired:
        return qtTrId("lc-agent-outcome-entry-expired");
    case CredentialOutcome::Unspecified:
    default:
        // Unspecified has copy of its own rather than sharing a generic: it is
        // what a recovered or synthesized terminal reports, which happens on a
        // healthy path (an agent restart mid-attempt), not only as a default.
        return qtTrId("lc-agent-outcome-unspecified");
    }
}

} // namespace librecelik::agent
