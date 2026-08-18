// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me
//
// Truthfulness guard for the version the build stamps into itself.
//
// Two version strings reach compiled code and they answer different questions.
// LIBRECELIK_VERSION is the numeric triple: it feeds CMake's own version
// comparisons and the macOS bundle keys, both of which reject anything else.
// LIBRECELIK_VERSION_FULL is what a user reads off the About dialog and the
// main-window banner, and it must describe the build that is actually running —
// a tree past its last release tag has to say so.
//
// The defect this file exists to prevent is not a crash, it is a lie: a full
// version written down as a literal instead of derived keeps advertising the
// release it was typed at while the tree moves on underneath it. The prefix
// assertion below is what catches that — a literal full version and a derived
// triple disagree the moment the triple moves.

#include <gtest/gtest.h>

#include <algorithm>
#include <string_view>

#include "config.h"

namespace {

// Two-step so the argument is macro-expanded before being stringified.
#define LC_VERSION_STRINGIFY_INNER(value) #value
#define LC_VERSION_STRINGIFY(value) LC_VERSION_STRINGIFY_INNER(value)

// The triple rebuilt from its own components. Equality with LIBRECELIK_VERSION
// is what proves the components and the joined string come from one derivation
// rather than from two places that can drift apart.
constexpr std::string_view kComposedTriple = LC_VERSION_STRINGIFY(LIBRECELIK_VERSION_MAJOR) "." LC_VERSION_STRINGIFY(
    LIBRECELIK_VERSION_MINOR) "." LC_VERSION_STRINGIFY(LIBRECELIK_VERSION_PATCH);

constexpr std::string_view kVersion = LIBRECELIK_VERSION;
constexpr std::string_view kVersionFull = LIBRECELIK_VERSION_FULL;

} // namespace

TEST(VersionStamp, NumericTripleIsWhatTheBundleKeysCanAccept)
{
    EXPECT_EQ(kVersion, kComposedTriple);
    // CFBundleVersion / CFBundleShortVersionString accept period-separated
    // integers and nothing else. This is the contract src/CMakeLists.txt relies
    // on when it spells those keys from the numeric components.
    EXPECT_TRUE(std::ranges::all_of(kVersion, [](char c) { return (c >= '0' && c <= '9') || c == '.'; }))
        << "numeric version \"" << kVersion << "\" carries a suffix; the macOS bundle keys will be rejected";
}

TEST(VersionStamp, FullVersionIsDerivedFromTheSameSourceAsTheTriple)
{
    ASSERT_FALSE(kVersionFull.empty());
    // The whole point: FULL extends the triple, it does not replace it. A FULL
    // string frozen as a literal parts company with the derived triple as soon
    // as the tree crosses a release, and that mismatch lands here.
    EXPECT_TRUE(kVersionFull.starts_with(kVersion))
        << "full version \"" << kVersionFull << "\" does not start with the derived triple \"" << kVersion
        << "\" — the full version is not being derived from git";
    if (kVersionFull.size() > kVersion.size()) {
        // Everything describe appends (commit distance, hash, dirty marker) and
        // every pre-release label is introduced by a hyphen; anything else means
        // the prefix match above was accidental (e.g. "4.2.01").
        EXPECT_EQ(kVersionFull[kVersion.size()], '-')
            << "full version \"" << kVersionFull << "\" continues past the triple without a separator";
    }
}

TEST(VersionStamp, VersionResolvedRatherThanFallingToTheLastResort)
{
    // 0.0.1 is GitVersion.cmake's last resort, reached only when neither git
    // tags nor the committed VERSION file could answer. VERSION is committed, so
    // a source drop of any shape has one — seeing the last resort here means the
    // fallback chain itself broke, which is exactly the state a clean-clone /
    // tarball build has to be protected from.
    EXPECT_NE(kVersion, "0.0.1") << "version fell through to the last resort; the VERSION file is missing or unparsed";
}
