#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Self-test for check-network-pins.sh.
#
# Each case is a fixture tree (a .github/workflows directory and, where the
# case needs one, a ci/pins directory) and the exit code the gate must give
# over it. The first case runs over this repository itself, so a gate that
# refuses everything cannot pass; the vacuum cases assert 2, never 0 and
# never 1, because a scan that found nothing to judge has not judged.
set -u

here="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
subject="$here/check-network-pins.sh"
repo="$(cd -- "$here/../.." && pwd)"
if [ ! -f "$subject" ]; then
    echo "FAIL  subject missing: $subject does not exist -- nothing to prove" >&2
    exit 1
fi

work="$(mktemp -d "${TMPDIR:-/var/tmp}/check-network-pins.XXXXXX")" || exit 2
trap 'rm -rf "$work"' EXIT

JASPER=63e106c80eb72af9fd4fa28772499ab0138b9994
OTHER=7c979692ede3914a481b859dbeb307dff48bf175

cases=0
red=0
fails=0

# check <name> <want-rc> <dir> [<text the output must carry>...]
check() {
    local name="$1" want="$2" dir="$3"
    shift 3
    cases=$((cases + 1))
    [ "$want" != 0 ] && red=$((red + 1))
    local out got
    out="$(bash "$subject" --root "$dir" 2>&1)"
    got=$?
    if [ "$got" != "$want" ]; then
        printf 'FAIL  %s: rc=%s, want %s\n' "$name" "$got" "$want"
        printf '%s\n' "$out" | sed 's/^/  | /'
        fails=$((fails + 1))
        return
    fi
    local needle
    for needle in "$@"; do
        if ! printf '%s' "$out" | grep -qF -- "$needle"; then
            printf 'FAIL  %s: rc=%s as wanted, but the output does not name "%s"\n' \
                "$name" "$got" "$needle"
            printf '%s\n' "$out" | sed 's/^/  | /'
            fails=$((fails + 1))
            return
        fi
    done
    printf 'ok    %s (rc=%s)\n' "$name" "$got"
}

# fixture <dir>: an empty tree with a workflows directory and a pin file.
fixture() {
    mkdir -p "$1/.github/workflows" "$1/ci/pins"
    printf '%s\n' '# <name> <url> <commit> <version>' \
        "jasper  https://github.com/jasper-software/jasper.git  $JASPER  version-4.2.9" \
        > "$1/ci/pins/jpeg2000.txt"
}

# A pinned block: reads the pin file and asserts the fetched HEAD.
pinned_block() {
    cat <<'YML'
name: w
on: push
jobs:
  b:
    runs-on: ubuntu-latest
    steps:
      - name: fetch
        run: |
          read -r _ url sha ver < <(grep '^jasper ' ci/pins/jpeg2000.txt)
          mkdir -p /tmp/jasper && git -C /tmp/jasper init -q .
          git -C /tmp/jasper remote add origin "$url"
          git -C /tmp/jasper fetch --depth 1 origin "$sha"
          git -C /tmp/jasper checkout --detach FETCH_HEAD
          [ "$(git -C /tmp/jasper rev-parse HEAD)" = "$sha" ] || exit 1
YML
}

unpinned_block() {
    cat <<'YML'
name: w
on: push
jobs:
  b:
    runs-on: ubuntu-latest
    steps:
      - name: fetch
        run: |
          git clone --depth 1 https://github.com/jasper-software/jasper.git /tmp/jasper
YML
}

# N0 -- this repository, as it stands. Anti-vacuum: a gate that fails
# everything passes every red case below and this one catches it.
check "N0 this repository is pinned" 0 "$repo"

# N1 -- a clone with no commit anywhere near it.
d="$work/n1"; fixture "$d"; unpinned_block > "$d/.github/workflows/ci.yml"
check "N1 clone without a pinned commit" 1 "$d" "ci.yml:9"

# N2 -- a commit is checked out, but no pin file names it.
d="$work/n2"; fixture "$d"
{ unpinned_block; printf '          git -C /tmp/jasper checkout %s\n' "$OTHER"
  printf '          [ "$(git -C /tmp/jasper rev-parse HEAD)" = "%s" ]\n' "$OTHER"; } \
    > "$d/.github/workflows/ci.yml"
check "N2 checked-out commit is in no pin file" 1 "$d" "$OTHER"

# N3 -- the pin file holds a tag name where the commit belongs.
d="$work/n3"; fixture "$d"; pinned_block > "$d/.github/workflows/ci.yml"
printf '%s\n' 'jasper  https://github.com/jasper-software/jasper.git  version-4.2.9  version-4.2.9' \
    > "$d/ci/pins/jpeg2000.txt"
check "N3 pin value is not a 40-hex commit" 1 "$d" "jpeg2000.txt:1"

# N4 -- pinned in the release workflow, cloned unpinned in CI: the two
# workflows build different bytes, and the message has to name both.
d="$work/n4"; fixture "$d"
pinned_block > "$d/.github/workflows/release.yml"
unpinned_block > "$d/.github/workflows/ci.yml"
check "N4 pin applied in one workflow only" 1 "$d" "release.yml" "ci.yml:9"

# N5 -- no clone and no fetch anywhere: nothing measured.
d="$work/n5"; fixture "$d"
printf '%s\n' 'name: w' 'on: push' 'jobs:' '  b:' '    runs-on: ubuntu-latest' \
    '    steps:' '      - run: echo hello' > "$d/.github/workflows/ci.yml"
check "N5 no network clone at all is unmeasured" 2 "$d"

# N6 -- a clone exists and there is no pin directory to judge it against.
d="$work/n6"; mkdir -p "$d/.github/workflows"; unpinned_block > "$d/.github/workflows/ci.yml"
check "N6 no ci/pins directory is unmeasured" 2 "$d"

# N7 -- the green shape, for the fixture: without it N1-N4 could all be
# passing for the wrong reason.
d="$work/n7"; fixture "$d"
pinned_block > "$d/.github/workflows/ci.yml"; pinned_block > "$d/.github/workflows/release.yml"
check "N7 both workflows pinned" 0 "$d"

# N8 -- the block reads the pin file but never asserts what it fetched.
d="$work/n8"; fixture "$d"
pinned_block | grep -v 'rev-parse HEAD' > "$d/.github/workflows/ci.yml"
cmp -s <(pinned_block) "$d/.github/workflows/ci.yml" \
    && { echo "FAIL  N8 fixture did not change -- the perturbation removed nothing"; fails=$((fails + 1)); }
check "N8 pinned fetch whose HEAD is never asserted" 1 "$d" "never asserted"

# The shape the workflows really use: a function that reads the pin row by
# the name it is called with, and two call sites.
function_block() {
    cat <<'YML'
name: w
on: push
jobs:
  b:
    runs-on: ubuntu-latest
    steps:
      - name: fetch
        run: |
          fetch_pinned() {  # fetch_pinned <name> <dir>
            local _n url sha ver got
            read -r _n url sha ver < <(grep "^$1 " ci/pins/jpeg2000.txt) || true
            [ -n "${sha:-}" ] || { echo "::error::no pin for $1"; exit 1; }
            mkdir -p "$2" && git -C "$2" init -q .
            git -C "$2" remote add origin "$url"
            git -C "$2" fetch --depth 1 origin "$sha"
            git -C "$2" checkout --detach FETCH_HEAD
            got="$(git -C "$2" rev-parse HEAD)"
            [ "$got" = "$sha" ] || { echo "::error::$1 pin $sha but HEAD $got"; exit 1; }
          }
          fetch_pinned jasper /tmp/jasper
          fetch_pinned qtimageformats /tmp/qtimageformats
YML
}
fixture2() {
    fixture "$1"
    printf '%s\n' "qtimageformats  https://code.qt.io/qt/qtimageformats.git  $OTHER  6.10.3" \
        >> "$1/ci/pins/jpeg2000.txt"
}

# N10 -- the function shape, both names pinned: green. Without it the three
# cases below could be failing on the shape rather than on the defect.
d="$work/n10"; fixture2 "$d"; function_block > "$d/.github/workflows/ci.yml"
check "N10 pinned fetch through a function, both names in the pin file" 0 "$d"

# N11 -- a second, unpinned clone inside the block that carries the pin. A
# verdict per block would call it pinned because its neighbour is.
d="$work/n11"; fixture2 "$d"
{ function_block; printf '%s\n' '          git clone --depth 1 https://github.com/example/evil.git /tmp/evil'; } \
    > "$d/.github/workflows/ci.yml"
check "N11 unpinned clone in a block that carries a pin" 1 "$d" "ci.yml:22" "example/evil.git"

# N12 -- the comparison deleted, the rev-parse line kept: the words are still
# there and nothing is asserted.
d="$work/n12"; fixture2 "$d"
function_block | grep -v '\[ "$got" = "$sha" \]' > "$d/.github/workflows/ci.yml"
cmp -s <(function_block) "$d/.github/workflows/ci.yml" \
    && { echo "FAIL  N12 fixture did not change"; fails=$((fails + 1)); }
check "N12 rev-parse kept, the comparison against the pin deleted" 1 "$d" "assert"

# N13 -- a name the block fetches is missing from the pin file: the fetch
# would fail at build time, and the gate has to say so on push.
d="$work/n13"; fixture "$d"; function_block > "$d/.github/workflows/ci.yml"
check "N13 a fetched name has no row in the pin file" 1 "$d" "qtimageformats"

# N9 -- no workflow directory: not a pass either.
d="$work/n9"; mkdir -p "$d/ci/pins"
check "N9 no workflows to read is unmeasured" 2 "$d"

if [ "$fails" -eq 0 ]; then
    echo "check-network-pins selftest: all cases passed"
else
    echo "check-network-pins selftest: $fails case(s) failed"
fi
printf 'selftest: %s cases, %s red-proved\n' "$cases" "$red"
[ "$fails" -eq 0 ]
