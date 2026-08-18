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
# "The repository's OWN tags" is load-bearing and is checked rather than assumed
# — see the ownership test below. Nothing here is allowed to fail the configure:
# every way of answering the question degrades to the next one and the last
# resort is a constant, because a version string is a label on the build, never
# a precondition for producing it.
#
# The stamp is refreshed by re-running configure, and the git state that feeds it
# is registered in CMAKE_CONFIGURE_DEPENDS so that happens on its own when the
# commit or the working tree moves. Without that registration the FIRST configure
# of a build tree froze the answer: the build stayed up to date, the generator
# reported no work to do, and the banner went on naming a commit the tree had
# long left behind — `-dirty` marker and all, on a clean checkout.

if(NOT DEFINED GIT_EXECUTABLE)
    find_package(Git QUIET REQUIRED)
endif()

# The repo root relative to THIS module (cmake/), not CMAKE_SOURCE_DIR: the
# module must describe its own repository no matter which directory the
# top-level build happens to start from. REALPATH collapses the `/..` and
# resolves symlinks on the way, because the ownership test below compares this
# against a path git prints already normalised — `<root>/cmake/..` is the same
# directory as `<root>` but never the same string.
get_filename_component(GIT_VERSION_SRC_DIR "${CMAKE_CURRENT_LIST_DIR}/.." REALPATH)

# Empty until something answers for it. Each source below fills it only with a
# string it has already checked carries a numeric triple, so the parse at the
# bottom cannot be handed a value that fails.
set(GIT_VERSION_FULL "")
set(GIT_VERSION_OWN_REPO FALSE)
set(GIT_VERSION_GIT_DIR "")

if(GIT_EXECUTABLE)
    # WHOSE repository is answering? git searches from the working directory
    # UPWARDS, so a source tree carrying no `.git` of its own — a release
    # tarball unpacked into an AUR `$srcdir`, a gbp build area, a Nix sandbox,
    # a vendored copy inside somebody else's checkout — is answered by whatever
    # repository happens to ENCLOSE it. `describe` then succeeds against a
    # completely unrelated project's tags and stamps that project's version
    # onto this build; and because it succeeded, the VERSION file that would
    # have given the right answer is never reached. The tags may only be
    # believed when the repository git found is this source tree itself.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --show-toplevel
        WORKING_DIRECTORY ${GIT_VERSION_SRC_DIR}
        OUTPUT_VARIABLE GIT_VERSION_TOPLEVEL
        RESULT_VARIABLE GIT_VERSION_TOPLEVEL_ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET # "not a git repository" is an ordinary answer here
        )
    if(NOT GIT_VERSION_TOPLEVEL_ERROR_CODE AND GIT_VERSION_TOPLEVEL)
        get_filename_component(GIT_VERSION_TOPLEVEL "${GIT_VERSION_TOPLEVEL}" REALPATH)
        if(GIT_VERSION_TOPLEVEL STREQUAL GIT_VERSION_SRC_DIR)
            set(GIT_VERSION_OWN_REPO TRUE)
        endif()
    endif()
endif()

if(GIT_VERSION_OWN_REPO)
    # Two deliberate departures from the plain `describe --tags`:
    #
    #  * NO `--abbrev=0`. That flag prints the bare tag name and nothing else,
    #    which is precisely what made every between-tags build advertise itself
    #    as the last release. Without it, describe appends `-<commits>-g<sha>`
    #    off-tag and prints the bare tag on a tagged commit.
    #  * `--match` restricted to VERSION-SHAPED tags: a leading digit and two
    #    further dot-separated groups that each start with one. This repository
    #    also carries local rollback/bookkeeping tags whose names are not
    #    version strings, and the older `[0-9]*` glob accepted every one of
    #    them that merely began with a digit — one `2026-08-18-backup` was
    #    enough to become "the version".
    execute_process(
        COMMAND ${GIT_EXECUTABLE} describe --tags --dirty
                --match "[0-9]*.[0-9]*.[0-9]*" --match "v[0-9]*.[0-9]*.[0-9]*"
        WORKING_DIRECTORY ${GIT_VERSION_SRC_DIR}
        OUTPUT_VARIABLE GIT_DESCRIBE_VERSION
        RESULT_VARIABLE GIT_DESCRIBE_ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET # silence git's "fatal: No names found" on a tagless clone
        )
    if(NOT GIT_DESCRIBE_ERROR_CODE AND GIT_DESCRIBE_VERSION)
        string(REGEX REPLACE "^v" "" GIT_DESCRIBE_VERSION "${GIT_DESCRIBE_VERSION}")
        # `--match` is an fnmatch glob, not a version grammar: `1a.2.3` passes
        # it and carries no triple. A tag nobody can parse is a bookkeeping
        # mistake in ONE clone, and it must not brick configure for everyone
        # who fetched it — name it and let the VERSION file answer instead.
        if(GIT_DESCRIBE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
            set(GIT_VERSION_FULL "${GIT_DESCRIBE_VERSION}")
        else()
            message(WARNING "Nearest git tag \"${GIT_DESCRIBE_VERSION}\" carries no MAJOR.MINOR.PATCH triple; "
                            "falling back to the VERSION file.")
        endif()
    endif()

    # `.git` is a FILE, not a directory, in a worktree and in a submodule, so
    # the state to watch has to be asked for rather than assumed to sit at
    # `${GIT_VERSION_SRC_DIR}/.git`.
    execute_process(
        COMMAND ${GIT_EXECUTABLE} rev-parse --absolute-git-dir
        WORKING_DIRECTORY ${GIT_VERSION_SRC_DIR}
        OUTPUT_VARIABLE GIT_VERSION_GIT_DIR
        RESULT_VARIABLE GIT_VERSION_GIT_DIR_ERROR_CODE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        )
    if(GIT_VERSION_GIT_DIR_ERROR_CODE)
        set(GIT_VERSION_GIT_DIR "")
    endif()
endif()

if(GIT_VERSION_GIT_DIR)
    # HEAD moves on a checkout, a branch switch and a detach; the index is
    # rewritten by every commit and every `git add`, which is what makes a
    # `-dirty` marker go stale. Touching either re-runs configure, and the
    # stamp is derived again from the tree that is actually there.
    foreach(GIT_VERSION_STAMP_INPUT IN ITEMS HEAD index)
        if(EXISTS "${GIT_VERSION_GIT_DIR}/${GIT_VERSION_STAMP_INPUT}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
                                                  "${GIT_VERSION_GIT_DIR}/${GIT_VERSION_STAMP_INPUT}")
        endif()
    endforeach()
    unset(GIT_VERSION_STAMP_INPUT)
endif()

if(GIT_VERSION_FULL STREQUAL "" AND EXISTS "${GIT_VERSION_SRC_DIR}/VERSION")
    # Release tarballs and GitHub source archives ship no .git tree, a
    # shallow/tagless clone has no tag to describe against, and a source drop
    # inside a foreign repository is refused the enclosing repo's tags above —
    # so `git describe` cannot answer for any of them. The committed VERSION
    # file is the authoritative fallback ahead of the last-resort default
    # below: it mirrors the most recent release tag and is bumped in lockstep
    # with each new tag (the release workflow gates that lockstep at tag time).
    file(STRINGS "${GIT_VERSION_SRC_DIR}/VERSION" GIT_VERSION_FULL LIMIT_COUNT 1)
    string(STRIP "${GIT_VERSION_FULL}" GIT_VERSION_FULL)
    string(REGEX REPLACE "^v" "" GIT_VERSION_FULL "${GIT_VERSION_FULL}")
    if(NOT GIT_VERSION_FULL MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+")
        message(WARNING "VERSION file holds \"${GIT_VERSION_FULL}\", which carries no MAJOR.MINOR.PATCH triple; "
                        "falling back to the default version.")
        set(GIT_VERSION_FULL "")
    endif()
endif()

if(GIT_VERSION_FULL STREQUAL "")
    set(GIT_VERSION_FULL 0.0.1)
    message(WARNING "Failed to determine the version from git tags or the VERSION file. "
                    "Using default version \"${GIT_VERSION_FULL}\".")
endif()

# The leading numeric triple, and only that. Anything the version string carries
# beyond it (pre-release label, commit distance, hash, dirty marker) belongs to
# GIT_VERSION_FULL alone — every consumer of the triple rejects a suffix.
string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" GIT_VERSION_NUMERIC_MATCH "${GIT_VERSION_FULL}")
if(NOT GIT_VERSION_NUMERIC_MATCH)
    # Not reachable from any input: every branch above either checked this same
    # pattern before assigning or assigned the constant. It stays as the guard
    # that keeps it that way — a future source of version strings that forgets
    # the check would otherwise leave the three components silently empty.
    message(FATAL_ERROR "Version string \"${GIT_VERSION_FULL}\" does not start with a MAJOR.MINOR.PATCH triple")
endif()
set(GIT_VERSION_MAJOR ${CMAKE_MATCH_1})
set(GIT_VERSION_MINOR ${CMAKE_MATCH_2})
set(GIT_VERSION_PATCH ${CMAKE_MATCH_3})

unset(GIT_VERSION_NUMERIC_MATCH)
unset(GIT_VERSION_SRC_DIR)
unset(GIT_VERSION_OWN_REPO)
unset(GIT_VERSION_TOPLEVEL)
unset(GIT_VERSION_TOPLEVEL_ERROR_CODE)
unset(GIT_VERSION_GIT_DIR)
unset(GIT_VERSION_GIT_DIR_ERROR_CODE)
unset(GIT_DESCRIBE_VERSION)
unset(GIT_DESCRIBE_ERROR_CODE)
