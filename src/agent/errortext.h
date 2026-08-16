// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

#pragma once
#include <LibreSCRS/AgentClient/CallError.h>
#include <LibreSCRS/AgentClient/CredentialTypes.h> // CredentialOutcome
#include <LibreSCRS/AgentClient/ErrorCode.h>
#include <LibreSCRS/AgentClient/OperationPhase.h>
#include <QString>

namespace librecelik::agent {

/// One localized line for a failed operation. Priority: known msgKey →
/// LC catalog string; unknown non-empty msgKey → msgFallback pass-through;
/// empty msgKey → per-code catalog string (CallError first when engaged —
/// the two are mutually exclusive on a failed op). Wire-frozen append-only:
/// unknown future codes fall to the generic string, never assert.
///
/// A blank msgFallback counts as no message throughout — it is non-empty to
/// QString and empty to a reader — so an unknown key that arrives without a
/// usable message resolves on the code axis instead of rendering nothing.
/// The result is never empty.
[[nodiscard]] QString errorText(LibreSCRS::AgentClient::ErrorCode code, LibreSCRS::AgentClient::CallError call,
                                const QString& msgKey, const QString& msgFallback);
[[nodiscard]] QString phaseText(LibreSCRS::AgentClient::OperationPhase phase);
[[nodiscard]] QString outcomeText(LibreSCRS::AgentClient::CredentialOutcome outcome);

} // namespace librecelik::agent
