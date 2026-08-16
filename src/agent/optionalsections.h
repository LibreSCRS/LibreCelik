// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// src/agent/optionalsections.h — the SINGLE F5 feature-gate decision,
// header-only so production (librecelik.cpp, Task 18) and the CI test
// (Task 19) compile the SAME code — the consumeBatchOutcome pattern: the
// gate must not exist only as untested window glue re-stated by its test.
#pragma once
#include "agent/agentgateway.h"
#include "agent/cardcontroller.h"

#include <QLatin1StringView>

namespace librecelik::agent {

inline constexpr QLatin1StringView kTokenInfoFeature{"token-info"};
inline constexpr QLatin1StringView kCredentialsFeature{"credentials"};

struct OptionalSections
{
    bool tokenInfo = false;   ///< verb dispatched; block may render
    bool credentials = false; ///< verb dispatched; rows/PIN verbs may render
};

/// Requests token info / credentials on the controller EXACTLY when the
/// gateway carries the feature token; an absent token requests NOTHING
/// (old agents stay silent — the controller-level double-guard emits the
/// empty-success shapes, never errorOccurred). The returned flags drive
/// the window's hide/show of the same surfaces, so dispatch and visibility
/// cannot drift apart.
inline OptionalSections requestOptionalSections(AgentGateway& gateway, CardController& controller)
{
    OptionalSections sections;
    if (gateway.hasFeature(kTokenInfoFeature)) {
        controller.requestTokenInfo();
        sections.tokenInfo = true;
    }
    if (gateway.hasFeature(kCredentialsFeature)) {
        controller.requestCredentials();
        sections.credentials = true;
    }
    return sections;
}

} // namespace librecelik::agent
