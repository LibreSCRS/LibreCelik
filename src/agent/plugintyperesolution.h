// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0

// src/agent/plugintyperesolution.h — the SINGLE GUI-plugin key decision,
// header-only so production (librecelik.cpp pluginFor) and the CI test
// compile the SAME code — the optionalsections.h pattern: the decision must
// not exist only as untested window glue re-stated by its test.
#pragma once
#include <LibreSCRS/AgentClient/AgentCapabilities.h>

#include <QLatin1StringView>
#include <QString>

#include <cstdint>

namespace librecelik::agent {

/// The registry key of the generic PKI/token page — the rendering the
/// middleware read path gave every card no family plugin claimed.
inline constexpr QLatin1StringView kGenericTokenCardType{"token"};

/// Resolves which GUI-plugin registry key a card renders through.
///
/// - A resolved card type names its plugin directly.
/// - Unresolved on a pre-card-type agent: that agent will never send a type
///   at all, so a PKI surface falls back to the generic token page (the
///   historical middleware-path rendering) rather than nothing.
/// - Unresolved on a card-type-capable agent: WAIT (empty result — the
///   window buffers groups and replays on cardTypeResolved) ONLY while
///   resolution is still possible. The agent resolves a card's type
///   authoritatively only from a completed ReadIdentity/GetPhoto
///   (Card1.xml), and it refuses both verbs without the IdentityData
///   capability — a multi-candidate card whose capability INTERSECTION
///   dropped IdentityData can therefore never resolve, and waiting is
///   provably unbounded (seen live: a CardEdge signing token spun
///   forever). Such a card is a PKI surface and nothing else: render the
///   generic token page.
///
/// Returns an empty string when there is nothing to render yet (type still
/// resolvable — wait for cardTypeResolved) or nothing to render at all (no
/// PKI surface; the caller shows its unsupported notice).
[[nodiscard]] inline QString effectiveGuiPluginKey(bool agentAdvertisesCardType, std::uint32_t capabilityBits,
                                                   const QString& resolvedCardType)
{
    if (!resolvedCardType.isEmpty())
        return resolvedCardType;

    if (!LibreSCRS::AgentClient::has(capabilityBits, LibreSCRS::AgentClient::Cap::Pki))
        return {};

    const bool typeCanStillResolve =
        agentAdvertisesCardType &&
        LibreSCRS::AgentClient::has(capabilityBits, LibreSCRS::AgentClient::Cap::IdentityData);
    return typeCanStillResolve ? QString() : QString(kGenericTokenCardType);
}

/// Whether a LATE type resolution must start the identity read the add-time
/// decision skipped.
///
/// The fallback above renders the generic token page immediately when
/// resolution cannot happen — and without the IdentityData capability the
/// window never starts the identity read at all. An agent may still resolve
/// the type afterwards (a restarted agent re-exports a seated card typeless,
/// carrying the candidate INTERSECTION until its own insertion probe
/// completes), and the token page must not be that card's final rendering:
/// the read this predicate orders is what re-plugins the page, through
/// identityReady's unconditional rebuild.
///
/// A read the window already started is never started twice — in the
/// ordinary multi-candidate wait the RUNNING read is what resolved the type.
/// And a resolved card that still carries no IdentityData has no read to
/// start: the token page is that card's whole surface.
[[nodiscard]] constexpr bool lateResolutionStartsRead(bool identityReadStarted, std::uint32_t capabilityBits) noexcept
{
    return !identityReadStarted &&
           LibreSCRS::AgentClient::has(capabilityBits, LibreSCRS::AgentClient::Cap::IdentityData);
}

} // namespace librecelik::agent
