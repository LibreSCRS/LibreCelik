# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
#
# Shared version resolution for the packaging scripts.
#
# Must agree with cmake/GitVersion.cmake, which answers the same question for
# the build: the two used to differ, and a comment in each claimed they did not.
# Keep them in step — the artefact file name and the version compiled into the
# binary are read side by side by anyone reporting a bug.
#
# Usage:  project_version "<project-root>"  -> prints the version, never fails
#
# shellcheck shell=bash

project_version() {
    local root="$1"
    local version=""

    # The repository must be THIS project's. `git describe` walks up from the
    # working directory, so an AUR $srcdir, a gbp export or a vendored copy
    # unpacked inside another checkout would otherwise answer with the
    # ENCLOSING project's newest tag and stamp the artefact with a version
    # belonging to somebody else.
    local toplevel
    toplevel="$(git -C "$root" rev-parse --show-toplevel 2>/dev/null || true)"
    if [ -n "$toplevel" ] && [ "$toplevel" = "$root" ]; then
        # Version-SHAPED tags only: a leading digit plus two further
        # dot-separated groups. `--match` is an fnmatch glob and not a version
        # grammar, so the shape is re-checked below rather than trusted.
        version="$(git -C "$root" describe --tags --abbrev=0 \
                       --match '[0-9]*.[0-9]*.[0-9]*' --match 'v[0-9]*.[0-9]*.[0-9]*' 2>/dev/null || true)"
        version="${version#v}"
        case "$version" in
            [0-9]*.[0-9]*.[0-9]*) ;;
            *) version="" ;;
        esac
    fi

    # No git, a foreign repository, or a tagless clone: the VERSION file is the
    # same authority the build falls back to. "dev" only when even that is
    # missing, so a source drop never silently names itself after nothing.
    if [ -z "$version" ] && [ -r "$root/VERSION" ]; then
        version="$(head -n1 "$root/VERSION" | tr -d '[:space:]')"
    fi

    printf '%s' "${version:-dev}"
}
