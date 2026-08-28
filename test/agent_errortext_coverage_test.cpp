// SPDX-License-Identifier: GPL-3.0-or-later
// SPDX-FileCopyrightText: 2026 hirashix0
//
// Total-coverage guard for the two failure taxonomies errorText() renders.
//
// This file is the only place in this repository where APPENDING a code fails
// the build, and that is deliberate. errortext.cpp does not re-declare either
// enumeration: it switches on the agent client library's own types, and the
// library decodes a value this build does not know through verbatim rather than
// rejecting it. Every switch over such a type is therefore obliged to carry a
// `default:` arm — which is exactly what stops -Wswitch from noticing an append.
// namedCodeText() and callText() both carry one on purpose: at RUN time an
// unknown code must fall through to whatever the agent itself said, not to a
// diagnostic.
//
// expectedKindFor() and expectedCallKindFor() below switch over EVERY
// enumerator with NO default arm, and this translation unit is compiled with
// -Werror=switch (see test/CMakeLists.txt). Rebuilding against a client library
// that appended a code therefore fails HERE, naming the new enumerator, and the
// fix is to decide its copy: phrase it in errortext.cpp, or consciously leave it
// to the agent-authored fallback, and classify it below either way.
//
// What this cannot do is notice a code the AGENT has but this build's client
// library does not: there is no build edge from here to the agent. That gate
// lives agent-side, where the enumeration is pinned value-for-value against the
// published wire contract and new codes land first.

#include "agent/errortext.h"

#include "qstring_printto.h"

#include <QCoreApplication>
#include <QSet>
#include <QString>
#include <QTranslator>

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

namespace {

using LibreSCRS::AgentClient::CallError;
using LibreSCRS::AgentClient::ErrorCode;

/// One recognisable string standing in for whatever the agent itself authored:
/// seeing it come back out means the resolution declined to say anything of its
/// own about the failure.
[[nodiscard]] QString sentinelMessage()
{
    return QStringLiteral("__agent_fallback_sentinel__");
}

/// Probe with the shape errorText() reduces to when only the code axis is
/// engaged: no key, no transport failure.
[[nodiscard]] QString forCode(ErrorCode code, const QString& agentMessage)
{
    return librecelik::agent::errorText(code, CallError::None, QString(), agentMessage);
}

enum class Kind {
    Localized,    // errortext.cpp phrases this code itself
    AgentFallback // errortext.cpp passes the agent-authored message through
};

// NO default case: with -Werror=switch an enumerator appended to the client
// library's ErrorCode is a compile error until it is classified here.
constexpr std::optional<Kind> expectedKindFor(ErrorCode code) noexcept
{
    switch (code) {
    case ErrorCode::None:
        // Not a classification at all; a failure that named no code has nothing
        // of its own to say, so whatever the agent wrote rides along.
        return Kind::AgentFallback;
    case ErrorCode::CardRemoved:
    case ErrorCode::CredentialWrong:
    case ErrorCode::CredentialBlocked:
    case ErrorCode::CommunicationError:
    case ErrorCode::ParseError:
    case ErrorCode::UnsupportedCard:
    case ErrorCode::AuthFailed:
    case ErrorCode::PrompterError:
    case ErrorCode::CapabilityMissing:
    case ErrorCode::WatchdogTimeout:
    case ErrorCode::KeyNotFound:
    case ErrorCode::KeyAmbiguous:
    case ErrorCode::CertExpiredBlocked:
    case ErrorCode::ChainIncomplete:
    case ErrorCode::TsaUnreachable:
    case ErrorCode::SigningEngineError:
    case ErrorCode::RateLimited:
    case ErrorCode::EngineUnavailable:
    case ErrorCode::InvalidDocument:
    case ErrorCode::EntryExpired:
        return Kind::Localized;
    }
    return std::nullopt; // past the enumerated tail (used to probe beyond it)
}

// Enumerated size derived from the classification switch itself (values outside
// it fall through to nullopt), so the walk below can never silently under-cover:
// the compiler forces the switch to track the enumeration, and the count tracks
// the switch.
constexpr std::uint32_t classifiedCount() noexcept
{
    std::uint32_t count = 0;
    while (expectedKindFor(static_cast<ErrorCode>(count)).has_value())
        ++count;
    return count;
}

constexpr std::uint32_t kClassifiedCount = classifiedCount();

// Keep this anchor on the LAST enumerator. It and -Werror=switch catch DIFFERENT
// halves of an append, and it takes both to force a deliberate decision:
//   - a code appended and NOT classified above trips -Werror=switch (the switch
//     stops being exhaustive) while this assert still holds;
//   - a code appended AND classified above, folded into an existing `case`
//     group, leaves the switch exhaustive — and then only this assert trips,
//     because the derived count outgrew the anchor.
// So neither check is redundant with the other.
static_assert(kClassifiedCount == static_cast<std::uint32_t>(ErrorCode::EntryExpired) + 1u,
              "classification switch out of step with the ErrorCode tail; move this anchor to the new last "
              "enumerator and classify the new code in expectedKindFor() in the same change");

// --- the second axis: CallError ----------------------------------------------

enum class CallKind {
    OwnCopy,    // errorText renders this classification's own localized copy
    NotAFailure // CallError::None — resolution continues past the transport arm
};

// NO default case, same as expectedKindFor above.
constexpr std::optional<CallKind> expectedCallKindFor(CallError call) noexcept
{
    switch (call) {
    case CallError::None:
        return CallKind::NotAFailure;
    case CallError::AgentUnavailable:
    case CallError::Timeout:
    case CallError::TransportFailure:
        return CallKind::OwnCopy;
    case CallError::AccessDenied:
    case CallError::InvalidArguments:
    case CallError::ProtocolError:
        // The answered-refusal trio: the agent classified the request and
        // authored the reason, so a present message beats this copy. The copy
        // still exists for the case where no message came with it, which is why
        // these are OwnCopy and not a fallback kind — the walk below probes them
        // with an empty message.
        return CallKind::OwnCopy;
    }
    return std::nullopt; // not a CallError value (used to probe past the end)
}

constexpr std::uint8_t callErrorCount() noexcept
{
    std::uint8_t count = 0;
    while (expectedCallKindFor(static_cast<CallError>(count)).has_value())
        ++count;
    return count;
}

constexpr std::uint8_t kCallErrorCount = callErrorCount();

static_assert(kCallErrorCount == static_cast<std::uint8_t>(CallError::ProtocolError) + 1u,
              "classification switch out of step with the CallError tail; move this anchor to the new last "
              "enumerator and classify the new classification in expectedCallKindFor() in the same change");

// Two values past each taxonomy's tail, standing in for a code an agent newer
// than this build can already send (the client library passes such a value
// through verbatim). Nothing here names them, so they are the shapes where the
// code arm must decline and resolution must keep going.
constexpr std::uint32_t kProbesBeyondTail = 2;
constexpr std::uint8_t kProbesBeyondCallTail = 2;

// Message shapes every walk crosses. The whitespace-only entries are not
// padding: they are non-empty to QString and blank to a user, and an error line
// is judged by the user's reading.
const QString kBlankish[] = {QString(), QStringLiteral("   "), QStringLiteral("\t \n"), sentinelMessage()};

/// The English catalog, so the assertions run against the copy a user actually
/// reads rather than against bare message ids.
class ErrorTextCoverage : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        translator = new QTranslator();
        const QString qmDir = QStringLiteral(LIBRECELIK_TRANSLATIONS_DIR_DEFAULT);
        ASSERT_TRUE(translator->load(QStringLiteral("LibreCelik_en"), qmDir))
            << "failed to load LibreCelik_en.qm from " << qmDir.toStdString();
        ASSERT_TRUE(QCoreApplication::installTranslator(translator));
    }

    static void TearDownTestSuite()
    {
        QCoreApplication::removeTranslator(translator);
        delete translator;
        translator = nullptr;
    }

    static QTranslator* translator;
};

QTranslator* ErrorTextCoverage::translator = nullptr;

} // namespace

// Every enumerated value must land in exactly the classified set: Localized ->
// non-empty copy of its own, never the agent's message and never the generic;
// AgentFallback -> the agent's message verbatim. Appending a code without
// touching errortext.cpp therefore fails loudly — at compile via -Werror=switch,
// then here if the classification and the implementation disagree.
TEST_F(ErrorTextCoverage, EveryEnumeratedCodeHasADeliberateOutcome)
{
    const QString sentinel = sentinelMessage();
    const QString generic = forCode(ErrorCode::None, QString());
    ASSERT_FALSE(generic.isEmpty()) << "the generic floor rendered nothing";

    QSet<QString> seen;
    for (std::uint32_t value = 0; value < kClassifiedCount; ++value) {
        const auto code = static_cast<ErrorCode>(value);
        const auto kind = expectedKindFor(code);
        ASSERT_TRUE(kind.has_value()) << "code " << value << " lost its classification";

        const QString text = forCode(code, sentinel);
        switch (*kind) {
        case Kind::Localized:
            EXPECT_FALSE(text.isEmpty()) << "code " << value << " is classified Localized but rendered nothing";
            EXPECT_NE(text, sentinel) << "code " << value
                                      << " is classified Localized but fell through to the agent message — phrase it "
                                         "in namedCodeText() or reclassify it AgentFallback here";
            EXPECT_NE(text, generic) << "code " << value
                                     << " is classified Localized but renders the generic; a user cannot tell it "
                                        "apart from an unclassified failure";
            {
                // The uniqueness property belongs to the HEADLINE — the code's
                // own copy. A present agent message may only ADD a detail line
                // below it, never replace or reword it.
                const QString headline = forCode(code, QString());
                EXPECT_FALSE(seen.contains(headline))
                    << "code " << value << " shares its copy with another code; a user cannot tell the two apart";
                seen.insert(headline);
                EXPECT_EQ(text.section(QLatin1Char('\n'), 0, 0), headline)
                    << "code " << value << " let the agent message displace or reword its own localized headline";
            }
            break;
        case Kind::AgentFallback:
            EXPECT_EQ(text, sentinel) << "code " << value
                                      << " is classified AgentFallback but rendered copy of its own — reclassify it "
                                         "Localized here";
            break;
        }
    }
}

// Forward compatibility: an agent newer than this client may already send the
// next code. Until this build names it, the agent-authored message must surface
// untouched — the behaviour namedCodeText()'s `default:` arm exists for,
// asserted rather than assumed.
TEST_F(ErrorTextCoverage, FirstValueBeyondTheTailFallsBackToTheAgentMessage)
{
    const QString sentinel = sentinelMessage();
    EXPECT_EQ(forCode(static_cast<ErrorCode>(kClassifiedCount), sentinel), sentinel);
}

// Every transport classification must carry copy of its own: non-empty, and
// distinct from every sibling and from the generic. Sharing copy between two
// classifications would be indistinguishable to a user, so it is a failure here
// rather than a stylistic remark.
TEST_F(ErrorTextCoverage, EveryCallErrorHasItsOwnLocalizedCopy)
{
    QSet<QString> seen;

    for (std::uint8_t value = 0; value < kCallErrorCount; ++value) {
        const auto call = static_cast<CallError>(value);
        const auto kind = expectedCallKindFor(call);
        ASSERT_TRUE(kind.has_value()) << "call error " << static_cast<int>(value) << " lost its classification";

        switch (*kind) {
        case CallKind::OwnCopy: {
            // Probed with NO agent message, which is the shape where this copy is
            // the only thing that can be rendered — the answered-refusal trio
            // prefers a present message and would otherwise mask a missing copy.
            const QString text = librecelik::agent::errorText(ErrorCode::None, call, QString(), QString());
            EXPECT_FALSE(text.isEmpty()) << "call error " << static_cast<int>(value) << " rendered nothing";
            EXPECT_FALSE(seen.contains(text))
                << "call error " << static_cast<int>(value)
                << " shares its copy with another classification; a user cannot tell the two apart";
            seen.insert(text);
            break;
        }
        case CallKind::NotAFailure: {
            // None must not consume the agent's message — resolution continues.
            const QString sentinel = sentinelMessage();
            EXPECT_EQ(librecelik::agent::errorText(ErrorCode::None, call, QString(), sentinel), sentinel)
                << "CallError::None swallowed the agent-authored message";
            break;
        }
        }
    }

    const QString generic = librecelik::agent::errorText(ErrorCode::None, CallError::None, QString(), QString());
    EXPECT_FALSE(seen.contains(generic)) << "the generic reuses a transport classification's copy, so an unclassified "
                                            "failure is indistinguishable from that classification";
}

// The never-blank invariant, asserted as an invariant: EVERY point in the
// cross-product of both enumerations — each probed past its own tail — crossed
// with every message shape. Exhaustive rather than sampled, because the defect
// this guards is a blank error line and one blank point is the whole defect.
TEST_F(ErrorTextCoverage, ErrorTextIsNeverBlankAnywhereInTheCrossProduct)
{
    constexpr std::uint32_t kMessageShapes = std::size(kBlankish);
    std::uint32_t points = 0;

    for (std::uint32_t codeValue = 0; codeValue < kClassifiedCount + kProbesBeyondTail; ++codeValue) {
        const auto code = static_cast<ErrorCode>(codeValue);
        for (std::uint8_t callValue = 0; callValue < kCallErrorCount + kProbesBeyondCallTail; ++callValue) {
            const auto call = static_cast<CallError>(callValue);
            for (const QString& message : kBlankish) {
                EXPECT_FALSE(librecelik::agent::errorText(code, call, QString(), message).trimmed().isEmpty())
                    << "blank error line at code " << codeValue << ", call error " << static_cast<int>(callValue)
                    << ", message \"" << message.toStdString() << '"';
                ++points;
            }
        }
    }

    // Guards the walk itself: a loop bound silently collapsing to zero would
    // otherwise leave every assertion above unexecuted and the test green.
    EXPECT_EQ(points,
              (kClassifiedCount + kProbesBeyondTail) * (kCallErrorCount + kProbesBeyondCallTail) * kMessageShapes);
    EXPECT_GT(points, 0u);
}
