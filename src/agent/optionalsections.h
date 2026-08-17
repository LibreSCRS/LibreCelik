// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// src/agent/optionalsections.h — the SINGLE F5 feature-gate decision,
// header-only so production (librecelik.cpp, Task 18) and the CI test
// (Task 19) compile the SAME code — the consumeBatchOutcome pattern: the
// gate must not exist only as untested window glue re-stated by its test.
#pragma once
#include "agent/agentgateway.h"
#include "agent/cardcontroller.h"

#include <LibreSCRS/AgentClient/AgentCapabilities.h>

#include <QLatin1StringView>

#include <cstdint>

namespace librecelik::agent {

inline constexpr QLatin1StringView kTokenInfoFeature{"token-info"};
inline constexpr QLatin1StringView kCredentialsFeature{"credentials"};

struct OptionalSections
{
    bool tokenInfo = false;   ///< verb dispatched; block may render
    bool credentials = false; ///< verb dispatched; rows/PIN verbs may render
};

/// Requests token info / credentials on the controller EXACTLY when the
/// gateway carries the feature token AND the card carries the capability the
/// agent itself gates the verb on (ReadTokenInfo refuses without Pki,
/// ListCredentials without PinManagement — per card, not per agent). A verb
/// the card provably cannot answer is never dispatched: the refusal would
/// come back through errorOccurred while the page is still the spinner,
/// which the window reads as a failed READ and releases the page the
/// still-running identity read was about to fill (the Leg-5 bench catch —
/// a pure eMRTD against a feature-capable agent). An absent token requests
/// NOTHING (old agents stay silent — the controller-level double-guard
/// emits the empty-success shapes, never errorOccurred). The returned flags
/// drive the window's hide/show of the same surfaces, so dispatch and
/// visibility cannot drift apart.
inline OptionalSections requestOptionalSections(AgentGateway& gateway, CardController& controller)
{
    namespace Cap = LibreSCRS::AgentClient::Cap;
    const std::uint32_t caps = controller.capabilityBits();
    OptionalSections sections;
    if (gateway.hasFeature(kTokenInfoFeature) && LibreSCRS::AgentClient::has(caps, Cap::Pki)) {
        controller.requestTokenInfo();
        sections.tokenInfo = true;
    }
    if (gateway.hasFeature(kCredentialsFeature) && LibreSCRS::AgentClient::has(caps, Cap::PinManagement)) {
        controller.requestCredentials();
        sections.credentials = true;
    }
    return sections;
}

} // namespace librecelik::agent
