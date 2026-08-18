# GitVersion
#
# Version derived from the repository's own git tags (https://semver.org/).
#
# GIT_VERSION_FULL   - The full, honest version string: the nearest release tag
#                      plus, when the tree is past that tag, the commit distance
#                      and abbreviated hash (`4.2.0-38-gda5a00c`), plus `-dirty`
#                      when the working tree has uncommitted changes. Exactly a
#                      release tag ONLY on a tagged commit with a clean tree.
# GIT_VERSION_MAJOR  - Major component of the leading numeric triple
# GIT_VERSION_MINOR  - Minor component
# GIT_VERSION_PATCH  - Patch component
#
# The numeric triple is the *only* thing that may feed `project(VERSION ...)`,
# CMake's own version comparisons and the macOS bundle version keys: all three
# reject a suffix. The full string is what user-facing banners show, because a
# development build that prints a bare release number is claiming to be a
# release it is not.
#
# All four variables are resolved once, at CONFIGURE time. A commit landing
# afterwards does not restamp an already-configured build tree; re-run cmake to
# refresh it. That is the same contract every git-stamped build carries and it
# is why the string is descriptive rather than authoritative.

if(NOT DEFINED GIT_EXECUTABLE)
    find_package(Git QUIET REQUIRED)
endif()

# The repo root relative to THIS module (cmake/), not CMAKE_SOURCE_DIR: the
# module must describe its own repository no matter which directory the
# top-level build happens to start from.
set(GIT_VERSION_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/..")

set(GIT_VERSION_RESOLVED FALSE)

if(GIT_EXECUTABLE)
    # Two deliberate departures from the plain `describe --tags`:
    #
    #  * NO `--abbrev=0`. That flag prints the bare tag name and nothing else,
    #    which is precisely what made every between-tags build advertise itself
    #    as the last release. Without it, describe appends `-<commits>-g<sha>`
    #    off-tag and prints the bare tag on a tagged commit.
    #  * `--match` restricted to version-shaped tags. This repository also
    #    carries local rollback/bookkeeping tags whose names are not version
    #    strings; letting one of those win would yield a "version" that no
    #    numeric parse can survive.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty --match "[0-9]*" --match "v[0-9]*"
        WORKING_DIRECTORY ${GIT_VERSION_SRC_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
        RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET # silence git's "fatal: No names found" on a tagless clone
        )
    if(NOT GIT_DESCRIBE_ERROR_CODE AND GIT_DESCRIBE_VERSION)
        string(REGEX REPLACE "^v" "" GIT_VERSION_FULL "${GIT_DESCRIBE_VERSION}")
        set(GIT_VERSION_RESOLVED TRUE)
    endif()
endif()

if(NOT GIT_VERSION_RESOLVED AND EXISTS "${GIT_VERSION_SRC_DIR}/VERSION")
    # Release tarballs and GitHub source archives ship no .git tree, and a
    # shallow/tagless clone has no tag to describe against, so `git describe`
    # above cannot answer. The committed VERSION file is the authoritative
    # fallback ahead of the last-resort default below: it mirrors the most
    # recent release tag and is bumped in lockstep with each new tag.
    file(STRINGS "${GIT_VERSION_SRC_DIR}/VERSION" GIT_VERSION_FULL LIMIT_COUNT 1)
    string(STRIP "${GIT_VERSION_FULL}" GIT_VERSION_FULL)
    string(REGEX REPLACE "^v" "" GIT_VERSION_FULL "${GIT_VERSION_FULL}")
    set(GIT_VERSION_RESOLVED TRUE)
endif()

if(NOT GIT_VERSION_RESOLVED)
    set(GIT_VERSION_FULL 0.0.1)
    message(WARNING "Failed to determine the version from git tags or the VERSION file. "
                    "Using default version \"${GIT_VERSION_FULL}\".")
endif()

# The leading numeric triple, and only that. Anything the version string carries
# beyond it (pre-release label, commit distance, hash, dirty marker) belongs to
# GIT_VERSION_FULL alone — every consumer of the triple rejects a suffix.
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" GIT_VERSION_NUMERIC_MATCH "${GIT_VERSION_FULL}")
if(NOT GIT_VERSION_NUMERIC_MATCH)
    message(FATAL_ERROR "Version string \"${GIT_VERSION_FULL}\" does not start with a MAJOR.MINOR.PATCH triple")
endif()
set(GIT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(GIT_VERSION_MINOR ${CMAKE_MATCH_2})
set(GIT_VERSION_PATCH ${CMAKE_MATCH_3})

unset(GIT_VERSION_RESOLVED)
unset(GIT_VERSION_NUMERIC_MATCH)
unset(GIT_VERSION_SRC_DIR)
