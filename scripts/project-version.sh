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
    # git reports the physical path; compare against the physical root too,
    # or a root reached through a symlink skips the tag, as the build does not.
    if [ -n "$toplevel" ] && [ "$toplevel" = "$(cd "$root" 2>/dev/null && pwd -P)" ]; then
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

    # The VERSION file is read unconditionally and the NEWER of the two wins --
    # the rule cmake/GitVersion.cmake implements, and this helper must agree
    # with it. VERSION is not a fallback but a floor: it is bumped at code
    # freeze, while the newest tag names the PREVIOUS release for the whole
    # cycle, so a tag-first answer labelled every artefact built before the tag
    # with the old number. The tag still wins when it is equal or ahead. With
    # no git, a foreign repository or a tagless clone, VERSION is all there is;
    # "dev" only when even that is missing, so a source drop never silently
    # names itself after nothing.
    local file=""
    if [ -r "$root/VERSION" ]; then
        file="$(head -n1 "$root/VERSION" | tr -d '[:space:]')"
        file="${file#v}"
        case "$file" in
            [0-9]*.[0-9]*.[0-9]*) ;;
            *) file="" ;;
        esac
    fi
    if [ -z "$version" ]; then
        version="$file"
    elif [ -n "$file" ] && _project_version_newer "$file" "$version"; then
        version="$file"
    fi

    printf '%s' "${version:-dev}"
}

# _project_version_newer A B: true when A's MAJOR.MINOR.PATCH is greater than
# B's. Numeric triples only, as in GitVersion.cmake: a pre-release label on the
# tag (5.0.0-rc2) must not decide the comparison. Plain arithmetic rather than
# `sort -V`, which the macOS packaging host is not guaranteed to have.
_project_version_newer() {
    local a b i
    IFS=. read -r -a a <<< "${1%%[!0-9.]*}"
    IFS=. read -r -a b <<< "${2%%[!0-9.]*}"
    for i in 0 1 2; do
        if (( 10#${a[i]:-0} > 10#${b[i]:-0} )); then return 0; fi
        if (( 10#${a[i]:-0} < 10#${b[i]:-0} )); then return 1; fi
    done
    return 1
}
