#!/usr/bin/env bash
# SPDX-License-Identifier: LGPL-2.1-or-later
# Selftest for check-tarball-determinism.sh. Six ways to build a tarball that
# is a fact about the machine rather than about the commit, plus the unmodified
# script as a control -- and one case where the script fails outright, because a
# check that reports success when it measured nothing is the vacuous kind.
#
# The fixture carries a tracked SYMLINK, because the mode arm shipped with a
# premise that was false for one ("a symlink is stored 777 by tar regardless of
# umask"): --mode rewrites it, and the arm refused the only tree in this stack
# that has one. The control case is what holds that premise to the measurement.
#
# The subject is not run against this repository: the fixture is a throwaway
# git repository holding a COPY of make-source-tarball.sh, which is what makes
# it perturbable at all. The fixture's top directory carries this repository's
# name, because the script maps that name to the Debian source name and refuses
# one it does not know.
#
# Each case asserts three things, because two are not enough:
#   * the perturbed script actually differs from the control (a perturbation
#     that changed nothing passes for the wrong reason);
#   * the exit code is the expected one;
#   * the named arm appears in the output. An exit code alone cannot tell a
#     refusal that worked from a refusal that fired on something else.
#
# /var/tmp, never /tmp, which is RAM on the maintainer machines.
#
# SPDX-License-Identifier: LGPL-2.1-or-later
set -u

here=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
subject="$here/check-tarball-determinism.sh"
maker="$here/make-source-tarball.sh"
[ -f "$subject" ] || { echo "missing subject: $subject" >&2; exit 2; }
[ -f "$maker" ]   || { echo "missing $maker" >&2; exit 2; }
name=$(basename "$(CDPATH= cd -- "$here/../.." && pwd)")

work=$(mktemp -d /var/tmp/tarball-determinism-selftest.XXXXXX) || exit 2
trap 'rm -rf "$work"' EXIT
fails=0
cases=0

# A fixture is a whole git repository, because the subject reads the commit
# timestamp out of one and the script clones one.
fixture() {  # fixture <dir>
    local d=$1
    mkdir -p "$d/ci/scripts" "$d/.github/workflows" "$d/src"
    printf '9.9.9\n' > "$d/VERSION"
    printf 'text\n' > "$d/src/a.txt"
    printf 'more\n' > "$d/README.md"
    # A tracked symlink. tar stores one 0777 from lstat and --mode rewrites
    # that to 0755, so the closed set the mode arm accepts has to be the image
    # of --mode and not the raw lstat mode.
    ln -s README.md "$d/README"
    # A shape whose readdir order and whose NAME order differ no matter what
    # the filesystem does with directory entries. git checks the index out in
    # path-byte order, so `src/a-b` is created before `src/a/`; tar sorts
    # directory-wise, so --sort=name emits `src/a/`, `src/a/c`, then `src/a-b`.
    # The filler entries make the two orders differ on a filesystem that hands
    # readdir back in hash order as well.
    mkdir -p "$d/src/a"
    printf 'inside a directory\n' > "$d/src/a/c"
    printf 'sorts before a/ under strcmp, after it under tar\n' > "$d/src/a-b"
    for i in $(seq -w 1 40); do printf 'filler\n' > "$d/src/n$i.txt"; done
    printf '#!/bin/sh\ntrue\n' > "$d/ci/scripts/run.sh"; chmod 755 "$d/ci/scripts/run.sh"
    printf 'name: x\non: [push]\n' > "$d/.github/workflows/x.yml"
    cp "$maker" "$d/ci/scripts/make-source-tarball.sh"; chmod 755 "$d/ci/scripts/make-source-tarball.sh"
    git -C "$d" init --quiet
    git -C "$d" add -A
    git -C "$d" -c user.name=selftest -c user.email=selftest@invalid -c commit.gpgsign=false \
        commit --quiet -m fixture
}

run() {  # run <case> <expected-rc> <arm> <sed-expression>
    local case=$1 want=$2 arm=$3 expr=$4 d="$work/$1/$name" got out
    cases=$((cases + 1))
    fixture "$d"
    if [ -n "$expr" ]; then
        sed -i "$expr" "$d/ci/scripts/make-source-tarball.sh"
        if cmp -s "$maker" "$d/ci/scripts/make-source-tarball.sh"; then
            echo "CASE $case: the perturbation changed nothing -- it would pass for the wrong reason"
            fails=$((fails + 1)); return
        fi
        git -C "$d" -c user.name=selftest -c user.email=selftest@invalid -c commit.gpgsign=false \
            commit --quiet -a -m perturb
    fi
    out=$(bash "$subject" "$d" 2>&1); got=$?
    if [ "$got" -ne "$want" ]; then
        echo "CASE $case: rc=$got, expected $want"
        printf '%s\n' "$out" | sed 's/^/    /'
        fails=$((fails + 1)); return
    fi
    case "$out" in
        *"$arm"*) : ;;
        *) echo "CASE $case: rc is right but no '$arm' line -- the refusal fired on something else"
           printf '%s\n' "$out" | sed 's/^/    /'
           fails=$((fails + 1)); return ;;
    esac
    printf '  ok    %-46s rc=%s  %s\n' "$case" "$got" "$arm"
}

# 1 -- the timestamps come from the clone. Two runs a second apart differ, two
#      inside one second do not, which is how this shipped for as long as the
#      script existed.
run wall_clock_mtime 1 'FAIL mtime' \
    's/--mtime="@\$epoch" --owner=0 --group=0 --numeric-owner //'

# 2 -- the mode is the caller's umask. The bytes stay a function of the commit
#      for anyone who happens to share the umask of whoever cut the release.
run umask_mode 1 'FAIL mode' "/--mode=/d"

# 3 -- the excludes are not anchored to the top directory, so they match no
#      member name tar ever writes and .github/ ships inside the tarball.
run unanchored_exclude 1 'FAIL github' \
    's|--exclude="\$name-\$version/\.github"|--exclude="./.github"|'

# 4 -- the top directory is the Debian source name. Every recipe cd-s into the
#      repository name, so the build fails at the first line of prepare().
run wrong_top_directory 1 'FAIL topdir' \
    's|^tree="\$work/\$name-\$version"|tree="$work/$src-$version"|'

# 5 -- the member order is readdir()'s, so it is a property of the filesystem
#      the clone landed on. Two runs on ONE machine still agree byte for byte,
#      which is why the comparison arm cannot see this and a packager
#      elsewhere is the first to find out.
run readdir_order 1 'FAIL order' 's/^tar --sort=name \\$/tar \\/'

# 6 -- the script leaves two candidate tarballs in the output directory. Which
#      one the release publishes is then a guess, and a check that measures the
#      one it happens to sort first is measuring nothing.
run two_candidates 2 'nothing could be measured' \
    's|^sha256sum "\$outdir/\${src}_\${version}.orig.tar.gz"|cp "$outdir/${src}_${version}.orig.tar.gz" "$outdir/spare_${version}.orig.tar.gz"\n&|'

# 7 -- the script fails. A check that says GREEN because it found no tarball is
#      worse than no check.
run script_fails 2 'nothing could be measured' \
    's|^set -euo pipefail|set -euo pipefail\nexit 3|'

# 8 -- control: the shipped script, unperturbed, must pass -- symlink and all.
run control 0 'tarball determinism: GREEN' ''

if [ "$fails" -eq 0 ]; then echo "check-tarball-determinism selftest: all $cases cases passed"; exit 0; fi
echo "check-tarball-determinism selftest: $fails of $cases case(s) failed"; exit 1
