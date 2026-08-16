// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Every string here is fetched with qtTrId() at CALL time. Nothing is cached
// in a static QString or a lookup table: a translator installed after this
// translation unit was first entered — which is exactly what a language switch
// at runtime is — must change what the next call returns.

#include "agent/errortext.h"

#include <QCoreApplication>

namespace librecelik::agent {

namespace {

using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::CredentialOutcome;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::OperationPhase;

/// Copy for the agent's wire-frozen error taxonomy.
///
/// The `default:` arm is REQUIRED, not stylistic: the client library decodes an
/// ErrorCode value this build does not know through verbatim rather than
/// rejecting it (see ErrorCode.h's append-tolerance note), so an unrecognised
/// value is opaque display data and must land on the generic line. ErrorCode::None
/// joins it there — a failure that named no code has nothing more specific to say.
[[nodiscard]] QString codeText(ErrorCode code)
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
    case ErrorCode::None:
    default:
        return qtTrId("lc-agent-error-generic");
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
[[nodiscard]] QString knownMsgKeyText(const QString& msgKey)
{
    if (msgKey == QLatin1StringView("op.card_removed"))
        return qtTrId("lc-agent-error-card-removed");
    if (msgKey == QLatin1StringView("op.watchdog_timeout"))
        return qtTrId("lc-agent-error-watchdog");
    return {};
}

} // namespace

QString errorText(ErrorCode code, CallError call, const QString& msgKey, const QString& msgFallback)
{
    // A whitespace-only agent message is not a message: non-empty to QString,
    // blank to a reader. Normalised once, here, so no arm below can hand a user
    // an empty error line.
    const QString message = msgFallback.trimmed().isEmpty() ? QString() : msgFallback;

    if (!msgKey.isEmpty()) {
        if (const QString named = knownMsgKeyText(msgKey); !named.isEmpty())
            return named;
        // Unknown key: the agent authored this text for exactly this failure,
        // so it beats anything the code axis could say about it. When the key
        // arrived without a usable message there is nothing to pass through and
        // resolution continues below rather than returning blank.
        if (!message.isEmpty())
            return message;
    }
    // The two classification axes are mutually exclusive on a failed op: when
    // the call never reached the agent, `code` is None and carries nothing.
    if (call != CallError::None)
        return callText(call);
    return codeText(code);
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
    case CredentialOutcome::Unspecified:
    default:
        // Unspecified has copy of its own rather than sharing a generic: it is
        // what a recovered or synthesized terminal reports, which happens on a
        // healthy path (an agent restart mid-attempt), not only as a default.
        return qtTrId("lc-agent-outcome-unspecified");
    }
}

} // namespace librecelik::agent
