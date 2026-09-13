#!/usr/bin/env bash
# check-tarball-determinism.sh [repository-root]
#
# The source tarball published with a release must be a function of the commit
# and of nothing else. Where the component carries an Arch recipe, that recipe
# holds the tarball's sha256 and a packager who rebuilds it has to get the same
# bytes back; where it does not, the tarball is still cosigned and listed in
# the signed SHA256SUMS beside the binaries, and anyone checking that manifest
# has to arrive at the same sum.
#
# Three separate facts about the machine used to leak into those bytes, and
# each of them survived for a different reason:
#
#   * every mtime came fresh from the clone, so two runs inside one second
#     agreed and two a second apart did not -- which is why a quick check
#     looked fine;
#   * owner and group were whoever ran the script;
#   * the MODE was the caller's umask. git clone honours it and tar records
#     what it finds, so the same commit tarred under umask 022 and under
#     umask 002 gave two different sums.
#
# So this does not compare two runs and stop there: a comparison alone is a
# coin toss on the wall clock, green whenever both runs land inside the same
# second. It forces a SECOND boundary and a UMASK boundary between the two
# runs, and then reads the property out of the archive itself -- every member
# carries `git log -1 --format=%ct` as its mtime, owner 0/0, and a mode from
# the closed set --mode writes; the members are in NAME order rather than the
# order readdir() happened to hand back; no member is under .github/, which the
# excludes silently failed to drop for as long as this script existed; and the
# top directory is <Repository>-<VERSION>, which is what the recipes cd into.
#
# The order arm is not redundant with the byte comparison, and that is the
# whole reason it is written out: two runs on ONE machine walk the tree in the
# same readdir() order, so dropping --sort=name leaves both runs identical and
# only a packager on another filesystem ever sees a different sum. Comparing
# the listing with `sort` would be wrong as well -- tar orders directory-wise,
# so a directory and a same-prefixed file interleave by depth rather than by
# byte: `a/c` precedes `a-b` because everything under `a/` sorts together,
# while plain strcmp puts `a-b` first because `-` (0x2D) sorts below `/`
# (0x2F). The selftest carries exactly that pair, `src/a-b` and `src/a/c`, and
# a plain `LC_ALL=C sort -c` calls it unsorted. The listing is compared
# against itself sorted with `/` mapped below every printable byte, which is
# exactly directory-wise order.
#
# What stays outside its reach is the compressor: gzip -9 output is a property
# of the gzip implementation, so the published sum is reproducible for anyone
# using the same one, not for everyone. (gzip is fed from a pipe, so it writes
# no name and a zero MTIME field either way.)
#
# Threat model. This runs the real script and measures the archive it produced,
# so it does not depend on the shape of the source text and cannot be talked
# out of a verdict by how the tar line is written. What it does not resist is a
# script that behaves differently for it than for a release: reading
# CI/GITHUB_ACTIONS, or the output directory's name, or the time of day. Nor
# does it see the release workflow -- that a workflow still builds and uploads
# the tarball at all is ci/scripts/check-artifact-names.sh's arm, not this
# one's. Code review, not this gate, catches a script written to mislead.
#
# Not every component that ships make-source-tarball.sh ships this check, and
# the components that do not do not carry the file at all. Here the clone the
# script makes is local and takes under a second; where the tree carries
# submodules or a dependency pinned for FetchContent, the same run pulls them
# from a third-party host, twice per check (measured: 28 s and 8 s per run),
# and a format-check that goes red because github.com is unreachable teaches
# people to ignore red. The property is measured wherever the clone is purely
# local, and stated as unmeasured in the release workflow of the rest.
#
# Exit: 0 the tarball is a function of the commit - 1 it is not
#       2 nothing could be measured, which is NOT a pass.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

repo=$(CDPATH= cd -- "${1:-.}" 2>/dev/null && pwd) || { echo "not a directory: ${1:-.}" >&2; exit 2; }
script="$repo/ci/scripts/make-source-tarball.sh"
[ -x "$script" ] || { echo "no executable $script -- nothing to measure, and that is not a pass" >&2; exit 2; }
[ -f "$repo/VERSION" ] || { echo "no $repo/VERSION -- nothing to measure, and that is not a pass" >&2; exit 2; }
git -C "$repo" rev-parse --git-dir >/dev/null 2>&1 || { echo "$repo is not a git repository -- nothing to measure" >&2; exit 2; }

name=$(basename "$repo")
version=$(tr -d '[:space:]' < "$repo/VERSION")
epoch=$(git -C "$repo" log -1 --format=%ct)
want=$(date -u -d "@$epoch" '+%Y-%m-%d %H:%M:%S' 2>/dev/null) || { echo "cannot format the commit timestamp" >&2; exit 2; }

# /var/tmp, not /tmp: /tmp is RAM on the maintainer machines and a tarball of a
# whole source tree does not belong there.
work=$(mktemp -d /var/tmp/tarball-determinism.XXXXXX) || exit 2
trap 'rm -rf "$work"' EXIT

( umask 022; "$script" "$work/a" ) >"$work/log-a" 2>&1
# A boundary, not a sleep: two runs inside one second agree even when every
# mtime is the wall clock, which is exactly the failure this exists to catch.
t0=$(date +%s); while [ "$(date +%s)" = "$t0" ]; do :; done
( umask 002; "$script" "$work/b" ) >"$work/log-b" 2>&1

shopt -s nullglob
A=("$work"/a/*.orig.tar.gz); B=("$work"/b/*.orig.tar.gz)
shopt -u nullglob
if [ "${#A[@]}" -ne 1 ] || [ "${#B[@]}" -ne 1 ]; then
    echo "FAIL run: make-source-tarball.sh produced ${#A[@]} and ${#B[@]} tarball(s), expected one each" >&2
    sed 's/^/    /' "$work/log-a" "$work/log-b" >&2
    echo "nothing could be measured, and that is not a pass" >&2
    exit 2
fi
a=${A[0]}; b=${B[0]}

rc=0
if cmp -s "$a" "$b"; then
    echo "bytes: identical across a second boundary and a umask boundary"
else
    echo "::error::FAIL bytes: two runs of make-source-tarball.sh differ ($(sha256sum <"$a" | cut -c1-16) vs $(sha256sum <"$b" | cut -c1-16)); the sha256sums line in the recipe would be a fact about one upload"
    rc=1
fi

census() {  # census <tarball> <label>
    local t=$1 label=$2 lst total okt oko okm gh first names sorted order
    lst=$(tar --utc --full-time -tvzf "$t") || return 2
    total=$(printf '%s\n' "$lst" | wc -l)
    okt=$(printf '%s\n' "$lst" | grep -cF " $want ")
    oko=$(printf '%s\n' "$lst" | awk '{print $2}' | grep -cxE '0/0|root/root')
    # The closed set is the IMAGE of --mode='go-w,a+rX': 755 for a directory
    # or anything executable, 644 for the rest -- and lrwxr-xr-x for a
    # symlink, because --mode rewrites the 0777 that lstat reports for one.
    # The premise this arm shipped with ("a symlink is stored 777 by tar
    # regardless of umask, so it belongs here too") was measured false: it
    # holds only for a tar line WITHOUT --mode, and this stack carries one
    # tracked symlink, in the vendored OpenSC tree, which the old set refused.
    okm=$(printf '%s\n' "$lst" | awk '{print $1}' | grep -cxE 'drwxr-xr-x|-rw-r--r--|-rwxr-xr-x|lrwxr-xr-x')
    # Matched under ANY top directory: whether the top directory is right is
    # a separate arm, and one wrong answer must not hide the other.
    gh=$(tar -tzf "$t" | grep -c '^[^/]*/\.github/')
    names=$(tar -tzf "$t")
    first=$(printf '%s\n' "$names" | head -1)
    # Directory-wise name order: map '/' to \1, which sorts below every byte a
    # path can otherwise hold, so `a/c` lands before `a-b` the way tar writes
    # it. A plain sort would report every correctly sorted tarball as unsorted.
    sorted=$(printf '%s\n' "$names" | tr '/' '\1' | LC_ALL=C sort | tr '\1' '/')
    order=ok; [ "$names" = "$sorted" ] || order=readdir
    printf '%s: members=%s mtime==%s:%s owner-0/0=%s mode-normalised=%s order=%s github=%s first=%s\n' \
        "$label" "$total" "$epoch" "$okt" "$oko" "$okm" "$order" "$gh" "$first"
    [ "$okt" = "$total" ] || { echo "::error::FAIL mtime ($label): $((total - okt)) of $total members do not carry the commit timestamp $want UTC -- the bytes are a fact about when the script ran"; rc=1; }
    [ "$oko" = "$total" ] || { echo "::error::FAIL owner ($label): $((total - oko)) of $total members are not owned by 0/0 -- the bytes are a fact about who ran the script"; rc=1; }
    if [ "$okm" != "$total" ]; then
        echo "::error::FAIL mode ($label): $((total - okm)) of $total members carry a mode outside the set --mode='go-w,a+rX' writes"
        echo "       only drwxr-xr-x, -rw-r--r--, -rwxr-xr-x and lrwxr-xr-x are that set; the mode census below names what is there instead"
        echo "       if the two runs also differ, that is the caller's umask; if they do not, it is a member --mode did not reach"
        rc=1
    fi
    if [ "$order" != ok ]; then
        echo "::error::FAIL order ($label): the members are in readdir() order, not name order"
        echo "       that order is a property of the filesystem the clone landed on, so two runs on one machine agree byte for byte"
        echo "       and the packager rebuilding elsewhere is the first to get a different sum; --sort=name is what fixes it"
        rc=1
    fi
    [ "$gh" = 0 ] || { echo "::error::FAIL github ($label): $gh member(s) under .github/ -- the excludes are not anchored to the top directory and match nothing"; rc=1; }
    [ "$first" = "$name-$version/" ] || { echo "::error::FAIL topdir ($label): first entry is '$first', expected '$name-$version/' -- the recipes cd into the repository name"; rc=1; }
    printf '%s modes: %s\n' "$label" "$(printf '%s\n' "$lst" | awk '{print $1}' | sort | uniq -c | tr '\n' ' ')"
}

census "$a" "umask-022" || exit 2
census "$b" "umask-002" || exit 2

if [ "$rc" -eq 0 ]; then
    echo "tarball determinism: GREEN -- $(basename "$a") is a function of $(git -C "$repo" rev-parse --short HEAD)"
else
    echo "tarball determinism: RED"
fi
exit "$rc"
