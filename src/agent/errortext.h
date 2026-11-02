// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/CallError.h>
#include <LibreSCRS/AgentClient/CredentialTypes.h> // CredentialOutcome
#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <QString>

#include <cstdint>
#include <optional>

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

/// @brief What the window does with a card whose read just failed.
///
/// Three outcomes, because two were not enough. The window used to choose
/// between dropping the card's page (right for a failure repeating cannot fix:
/// it stops one failure becoming a retry storm) and doing NOTHING — and every
/// failure the holder could actually clear fell into "nothing", leaving the
/// page spinning under "confirm the access number in the system window" long
/// after that window was gone, while the status bar said to try again and
/// offered no way to.
///
/// So a recoverable failure now gets its own outcome: stop the spinner and
/// stand there with a button.
enum class ReadFailureAction : std::uint8_t {
    LeaveAlone,   ///< The page already shows real card data; a late failure must not tear it down.
    LatchAndDrop, ///< Nothing was rendered and repeating cannot help: drop the page, remember the card.
    OfferRetry,   ///< Nothing was rendered but the holder can clear this: offer to read again.
};

/// @param pageIsSpinner Whether the card's page is still the read-in-progress
///                      placeholder — i.e. the read produced nothing to show.
/// @param code          The agent's taxonomy value for the failure.
///
/// Lives here rather than inside the window's signal handler for the reason
/// `requestOptionalSections` and `effectiveGuiPluginKey` do: no test binary
/// links the window, so a decision left in that handler is a decision no gate
/// can reach.
[[nodiscard]] ReadFailureAction readFailureAction(bool pageIsSpinner, LibreSCRS::AgentClient::ErrorCode code);

/// Localized text for a failed operation.
///
/// WHICH of the four facts a user reads is the client library's decision
/// (`LibreSCRS::AgentClient::describeFailure()`, the same one every client of
/// the agent makes); this function only supplies LibreCelik's sentences for
/// it: the catalog entry for a message key, the catalog entry for an error
/// code as the headline with the agent's own detail below it, the agent's
/// message, the catalog entry for a transport failure, or the generic line.
/// The result is never empty.
[[nodiscard]] QString errorText(LibreSCRS::AgentClient::ErrorCode code, LibreSCRS::AgentClient::CallError call,
                                const QString& msgKey, const QString& msgFallback);
/// Holder guidance for a credential (why it is blocked, who activates a key):
/// LibreCelik's own sentence for a guidance key it names, the agent's authored
/// fallback for any other key, nothing when neither says anything. The one
/// rule for every place a credential is shown.
[[nodiscard]] QString credentialGuidanceText(const std::optional<QString>& key, const std::optional<QString>& fallback);
/// @brief The line for a verb issued against a card the client has already removed.
[[nodiscard]] QString cardGoneText();
[[nodiscard]] QString phaseText(LibreSCRS::AgentClient::OperationPhase phase);
[[nodiscard]] QString outcomeText(LibreSCRS::AgentClient::CredentialOutcome outcome);

} // namespace librecelik::agent
