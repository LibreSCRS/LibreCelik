// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Every string here is fetched with qtTrId() at CALL time. Nothing is cached
// in a static QString or a lookup table: a translator installed after this
// translation unit was first entered — which is exactly what a language switch
// at runtime is — must change what the next call returns.

#include "agent/errortext.h"

#include <LibreSCRS/AgentClient/FailureCopy.h>

#include <QDebug>

#include <QCoreApplication>

namespace librecelik::agent {

namespace {

using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::CredentialOutcome;
using LibreSCRS::AgentClient::ErrorCode;
using LibreSCRS::AgentClient::FailureCopy;
using LibreSCRS::AgentClient::FailureSource;
using LibreSCRS::AgentClient::OperationPhase;

/// LC copy for the agent's wire-frozen error taxonomy. A code this build does
/// not name — the client library passes an unrecognised value through rather
/// than rejecting it — gets the generic line; the agent's detail below it
/// still says what happened.
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
    case ErrorCode::EntryExpired:
        return qtTrId("lc-agent-error-entry-expired");
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

/// LC copy for each of the agent's message keys the client library names in
/// `kLocalizedMessageKeys` — the only keys `describeFailure()` ever selects.
[[nodiscard]] QString messageKeyText(const QString& msgKey)
{
    if (msgKey == QLatin1StringView("op.card_removed"))
        return qtTrId("lc-agent-error-card-removed");
    if (msgKey == QLatin1StringView("op.watchdog_timeout"))
        return qtTrId("lc-agent-error-watchdog");
    if (msgKey == QLatin1StringView("op.cancelled"))
        return qtTrId("lc-agent-msg-cancelled");
    if (msgKey == QLatin1StringView("op.entry_expired"))
        return qtTrId("lc-agent-error-entry-expired");
    if (msgKey == QLatin1StringView("op.helper_too_old"))
        return qtTrId("lc-agent-error-helper-too-old");
    return qtTrId("lc-agent-error-generic");
}

} // namespace

bool isRetryableReadFailure(ErrorCode code)
{
    switch (code) {
    case ErrorCode::EntryExpired:
    case ErrorCode::CredentialWrong:
    case ErrorCode::PrompterError:
    case ErrorCode::CapabilityMissing:
    case ErrorCode::None:
        return true;
    default:
        // Deliberately a default, not an exhaustive switch: an appended code is
        // treated as NOT retryable, so the retry-storm guard stays intact until
        // someone decides otherwise. The client library also passes an
        // unrecognised numeric value through verbatim, so this arm is reachable.
        return false;
    }
}

ReadFailureAction readFailureAction(bool pageIsSpinner, ErrorCode code)
{
    if (!pageIsSpinner) {
        // Groups already streamed onto the page. A failure arriving now costs
        // the holder data they can already read; leave it standing.
        return ReadFailureAction::LeaveAlone;
    }
    return isRetryableReadFailure(code) ? ReadFailureAction::OfferRetry : ReadFailureAction::LatchAndDrop;
}

QString errorText(ErrorCode code, CallError call, const QString& msgKey, const QString& msgFallback)
{
    const FailureCopy copy = describeFailure(code, call, msgKey, msgFallback);
    switch (copy.source) {
    case FailureSource::MessageKey:
        return messageKeyText(copy.messageKey);
    case FailureSource::ErrorCode: {
        const QString headline = codeText(copy.errorCode);
        if (copy.detail.isEmpty() || copy.detail == headline)
            return headline;
        // Plain qWarning, not a category: this TU is compiled into test targets
        // that do not carry the logging TU, and the line must not cost them a
        // link dependency.
        qWarning() << "agent detail behind localized error" << static_cast<quint32>(copy.errorCode) << ":"
                   << copy.detail;
        return headline + QLatin1Char('\n') + copy.detail;
    }
    case FailureSource::AgentMessage:
        return copy.detail;
    case FailureSource::Transport:
        return callText(copy.callError);
    case FailureSource::Generic:
        break;
    }
    return qtTrId("lc-agent-error-generic");
}

QString credentialGuidanceText(const std::optional<QString>& key, const std::optional<QString>& fallback)
{
    // The guidance vocabulary is the agent's and grows independently of this
    // build, which is why the fallback arm exists and is the common one.
    const QString wireKey = key.value_or(QString());
    if (wireKey == QLatin1StringView("librescrs.pin.blocked.issuer"))
        return qtTrId("lc-pin-guidance-blocked-issuer");
    if (wireKey == QLatin1StringView("librescrs.pin.keyActivation.issuer"))
        return qtTrId("lc-pin-guidance-key-activation-issuer");
    const QString authored = fallback.value_or(QString());
    return authored.trimmed().isEmpty() ? QString() : authored;
}

QString cardGoneText()
{
    return errorText(ErrorCode::CardRemoved, CallError::None, {}, {});
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
