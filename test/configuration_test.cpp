// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright hirashix0@proton.me
//
// Truthfulness guard for the version the build stamps into itself.
//
// LIBRECELIK_VERSION_FULL is what a user reads off the About dialog and the
// main-window banner, and it must describe the build that is actually running —
// a tree past its last release tag has to say so, and the release it names has
// to be the one the committed VERSION file, the changelog and the packaging
// state.
//
// The defect this file exists to prevent is not a crash, it is a lie: a full
// version written down as a literal instead of derived keeps advertising the
// release it was typed at while the tree moves on underneath it. The prefix
// assertion below is what catches that — a literal full version and the
// VERSION file disagree the moment VERSION moves.

#include <gtest/gtest.h>

#include <string_view>

#include "config.h"

namespace {

// The committed VERSION file, read by the test build on its own.
constexpr std::string_view kVersionFile = LIBRECELIK_TEST_VERSION_FILE;
constexpr std::string_view kVersionFull = LIBRECELIK_VERSION_FULL;

} // namespace

TEST(VersionStamp, FullVersionNamesTheCommittedVersionFile)
{
    ASSERT_FALSE(kVersionFile.empty()) << "the test build could not read the VERSION file";
    ASSERT_FALSE(kVersionFull.empty());
    // The whole point: FULL extends the version the tree is heading for, it
    // does not replace it. A FULL string frozen as a literal parts company
    // with VERSION as soon as the tree crosses a release, and that mismatch
    // lands here.
    EXPECT_TRUE(kVersionFull.starts_with(kVersionFile))
        << "full version \"" << kVersionFull << "\" does not start with the VERSION file's \"" << kVersionFile
        << "\" — the full version is not being derived from it";
    if (kVersionFull.size() > kVersionFile.size()) {
        // Everything describe appends (commit distance, hash, dirty marker) and
        // every pre-release label is introduced by a hyphen; anything else means
        // the prefix match above was accidental (e.g. "4.2.01").
        EXPECT_EQ(kVersionFull[kVersionFile.size()], '-')
            << "full version \"" << kVersionFull << "\" continues past the VERSION file without a separator";
    }
}

TEST(VersionStamp, VersionResolvedRatherThanFallingToTheLastResort)
{
    // 0.0.1 is GitVersion.cmake's last resort, reached only when neither git
    // tags nor the committed VERSION file could answer. VERSION is committed, so
    // a source drop of any shape has one — seeing the last resort here means the
    // fallback chain itself broke, which is exactly the state a clean-clone /
    // tarball build has to be protected from.
    EXPECT_FALSE(kVersionFull.starts_with("0.0.1"))
        << "version fell through to the last resort; the VERSION file is missing or unparsed";
}
