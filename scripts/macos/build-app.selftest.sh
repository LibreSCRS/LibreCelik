#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# selftest-platforms: darwin
# (build-app.sh signs with codesign and edits entitlements with PlistBuddy;
# neither exists off macOS. codesign itself is faked here, PlistBuddy is not.)
#
# Self-test for scripts/macos/build-app.sh: its signing plan and its signing
# order, without a build and without a Developer ID. A fake codesign on PATH
# logs every call and answers `codesign -dv` with a TeamIdentifier the case
# chooses, which is how the check after signing is driven red. Every rule is
# then deleted from a copy of the script, and the case that guards it must go
# red over that copy.
#
# Bash 3.2 compatible, like the script it tests.
set -uo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$HERE/../.." && pwd)"
SCRIPT="$HERE/build-app.sh"

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT

cases=0
failed=0
red=0
red_expected=0

# ---------------------------------------------------------------- fixtures
mkdir -p "$work/bin"
cat > "$work/bin/codesign" <<'EOF'
#!/bin/sh
# Fake codesign: log the call; answer -dv with $FAKE_TEAM.
printf 'codesign' >> "$CODESIGN_LOG"
prev=""
for a in "$@"; do
    printf ' %s' "$a" >> "$CODESIGN_LOG"
    if [ "$prev" = "--entitlements" ]; then ents="$a"; fi
    prev="$a"
done
printf '\n' >> "$CODESIGN_LOG"
if [ -n "${ents:-}" ]; then
    { echo "ENTITLEMENTS-BEGIN"; cat "$ents"; echo "ENTITLEMENTS-END"; } >> "$CODESIGN_LOG"
fi
if [ "${1:-}" = "-dv" ]; then
    echo "Executable=$2" >&2
    echo "TeamIdentifier=${FAKE_TEAM:-not set}" >&2
fi
exit 0
EOF
chmod +x "$work/bin/codesign"

mkbuild() {  # mkbuild <dir> <team>   (team "NONE" = no cache entry at all)
    mkdir -p "$1"
    {
        echo "CMAKE_BUILD_TYPE:STRING=Release"
        [ "$2" = NONE ] || echo "LIBRESCRS_TEAM_ID:STRING=$2"
    } > "$1/CMakeCache.txt"
}
mkbuild "$work/b-none" ""
mkbuild "$work/b-team" "ABCDE12345"
mkbuild "$work/b-noentry" NONE

mkapp() {  # mkapp <app>
    local c="$1/Contents"
    mkdir -p "$c/MacOS" "$c/Frameworks/QtCore.framework/Versions/A" \
        "$c/PlugIns/platforms" "$c/PlugIns/gui-plugins"
    printf 'x' > "$c/MacOS/LibreCelik"; chmod +x "$c/MacOS/LibreCelik"
    printf 'x' > "$c/Frameworks/QtCore.framework/Versions/A/QtCore"
    printf 'x' > "$c/Frameworks/QtCore.framework/Versions/A/libinside.dylib"
    printf 'x' > "$c/Frameworks/liblibrescrs-agentclient-qt.1.dylib"
    printf 'x' > "$c/PlugIns/platforms/libqcocoa.dylib"
    printf 'x' > "$c/PlugIns/gui-plugins/eid-gui-plugin.so"
}

# A copy of the repository's two inputs, for mutants: the script finds the
# entitlements relative to itself.
mutant() {  # mutant <name> <sed-expression>  -> sets $m to the mutant's path
    local d="$work/mut-$1"
    mkdir -p "$d/scripts/macos" "$d/resources/macos"
    cp "$ROOT/resources/macos/LibreCelik.entitlements" "$d/resources/macos/"
    sed -e "$2" "$SCRIPT" > "$d/scripts/macos/build-app.sh"
    chmod +x "$d/scripts/macos/build-app.sh"
    if cmp -s "$SCRIPT" "$d/scripts/macos/build-app.sh"; then
        echo "MUTANT $1 changed nothing -- the selftest no longer matches the script" >&2
        failed=$((failed + 1))
    fi
    m="$d/scripts/macos/build-app.sh"
}

# ---------------------------------------------------------------- runner
# run <script> <env...> -- <args...>: sets $rc and $out (stdout+stderr), and
# $log (the fake codesign's log). PATH has the fake codesign first; HOME is
# empty so no ~/Qt is found; MACDEPLOYQT and LIBRESCRS_TEAM_ID are unset
# unless a case sets them.
run() {
    local script="$1"; shift
    local envs=()
    while [ "$1" != "--" ]; do envs+=("$1"); shift; done
    shift
    : > "$work/codesign.log"
    rm -rf "$work/app"; mkapp "$work/app"
    out="$(env -u MACDEPLOYQT -u LIBRESCRS_TEAM_ID -u CODESIGN_IDENTITY \
        PATH="$work/bin:/usr/bin:/bin" HOME="$work/nohome" CODESIGN_LOG="$work/codesign.log" \
        ${envs[@]+"${envs[@]}"} "$script" "$@" 2>&1)"
    rc=$?
    log="$(cat "$work/codesign.log")"
}

# expect <name> <want-rc> [<pattern>|!<pattern>]... : judge the last run.
# Returns 0 when the case passes. Patterns are grep -E over $out, or over
# $log when prefixed "log:"; a leading "!" (after any "log:") forbids a match.
judge() {
    local want="$1"; shift
    [ "$rc" = "$want" ] || { why="exit $rc, wanted $want"; return 1; }
    local p hay neg
    for p in "$@"; do
        neg=0; hay="$out"
        case "$p" in log:*) hay="$log"; p="${p#log:}" ;; esac
        case "$p" in '!'*) neg=1; p="${p#!}" ;; esac
        if printf '%s\n' "$hay" | grep -Eq -- "$p"; then
            [ "$neg" = 0 ] || { why="matched forbidden /$p/"; return 1; }
        else
            [ "$neg" = 1 ] || { why="no match for /$p/"; return 1; }
        fi
    done
    return 0
}
expect() {
    local name="$1"; shift
    cases=$((cases + 1))
    if judge "$@"; then
        echo "ok    $name"
    else
        echo "FAIL  $name: $why"
        printf '%s\n' "$out" | tail -n 5 | sed 's/^/  | /'
        failed=$((failed + 1))
    fi
}
# expect_red <name> <args as expect>: the case must FAIL over a mutant.
expect_red() {
    local name="$1"; shift
    red_expected=$((red_expected + 1))
    if judge "$@"; then
        echo "FAIL  red: $name -- the case stayed green over the mutant"
        failed=$((failed + 1))
    else
        echo "red   $name ($why)"
        red=$((red + 1))
    fi
}

# ---------------------------------------------------------------- the plan
run "$SCRIPT" -- --print-sign-plan "$work/b-none"
expect "P1 no team, ad hoc: plan" 0 '^identity=-$' '^timestamp=no$' '^dlv=yes$'

run "$SCRIPT" -- --print-sign-plan "$work/b-team"
expect "P2 team + ad hoc: refused" 1 'ad hoc'

run "$SCRIPT" CODESIGN_IDENTITY="Fake Identity" -- --print-sign-plan "$work/b-team"
expect "P3 team + identity: timestamp, library validation" 0 \
    '^identity=Fake Identity$' '^team=ABCDE12345$' '^timestamp=yes$' '^dlv=no$'

run "$SCRIPT" CODESIGN_IDENTITY="Fake Identity" -- --print-sign-plan "$work/b-none"
expect "P4 no team + identity: planned (the check after signing decides)" 0 \
    '^timestamp=yes$' '^dlv=yes$'

run "$SCRIPT" LIBRESCRS_TEAM_ID=ZZZZZ99999 CODESIGN_IDENTITY="Fake Identity" -- --print-sign-plan "$work/b-team"
expect "P5 environment team differs from the configured one: refused" 1 'in the environment'

run "$SCRIPT" LIBRESCRS_TEAM_ID=ABCDE12345 CODESIGN_IDENTITY="Fake Identity" -- --print-sign-plan "$work/b-team"
expect "P6 environment team equals the configured one: planned" 0 '^dlv=no$'

run "$SCRIPT" -- --print-sign-plan "$work/no-such-build"
expect "P7 no CMakeCache.txt: refused" 1 'no CMakeCache.txt'

run "$SCRIPT" -- --print-sign-plan "$work/b-noentry"
expect "P8 cache without LIBRESCRS_TEAM_ID: refused" 1 'carries no LIBRESCRS_TEAM_ID'

run "$SCRIPT" -- --print-sign-plan "$work/b-none"
expect "P9 the plan needs neither macdeployqt nor a version" 0 '!macdeployqt' '!version'

# ---------------------------------------------------------------- signing
run "$SCRIPT" -- --sign-bundle "$work/b-none" "$work/app"
expect "S1 ad hoc: no timestamp, disable-library-validation, no team check" 0 \
    'log:--timestamp=none' 'log:disable-library-validation' 'log:!-dv' 'log:!--deep --force'

run "$SCRIPT" CODESIGN_IDENTITY="Fake Identity" FAKE_TEAM=ABCDE12345 -- --sign-bundle "$work/b-team" "$work/app"
expect "S2 team identity, signed by that team: accepted, timestamped, validated" 0 \
    'log:--timestamp( |$)' 'log:!--timestamp=none' 'log:!disable-library-validation' 'log:-dv '
ok_order=1
order="$(printf '%s\n' "$log" | sed -n 's/^codesign --force .* \([^ ]*\)$/\1/p')"
last="$(printf '%s\n' "$order" | tail -n 1)"
fw_line="$(printf '%s\n' "$order" | grep -n 'QtCore.framework/Versions/A$' | cut -d: -f1)"
lib_lines="$(printf '%s\n' "$order" | grep -nE '\.(dylib|so)$' | cut -d: -f1)"
cases=$((cases + 1))
[ "$last" = "$work/app" ] || ok_order=0
[ -n "$fw_line" ] || ok_order=0
for l in $lib_lines; do [ "$l" -lt "${fw_line:-0}" ] || ok_order=0; done
[ "$(printf '%s\n' "$lib_lines" | grep -c .)" = 3 ] || ok_order=0
printf '%s\n' "$order" | grep -q 'libinside.dylib' && ok_order=0
[ "$(printf '%s\n' "$log" | grep -c -- '--entitlements')" = 1 ] || ok_order=0
if [ "$ok_order" = 1 ]; then
    echo "ok    S3 inside out: libraries, then frameworks by Versions/A, the bundle last with the only entitlements"
else
    echo "FAIL  S3 inside out: signing order is"; printf '%s\n' "$order" | sed 's/^/  | /'
    failed=$((failed + 1))
fi

run "$SCRIPT" CODESIGN_IDENTITY="Fake Identity" FAKE_TEAM=XYZXYZ0000 -- --sign-bundle "$work/b-team" "$work/app"
expect "S4 team identity, signed by another team: refused after signing" 1 "signed by team 'XYZXYZ0000'"

run "$SCRIPT" CODESIGN_IDENTITY="Fake Identity" FAKE_TEAM=XYZXYZ0000 -- --sign-bundle "$work/b-none" "$work/app"
expect "S5 identity with a team, build names none: refused after signing" 1 'never check who signed'

# ---------------------------------------------------------------- red
mutant adhoc '/\[ "\$CODESIGN_IDENTITY" = "-" \] && \[ -n "\$team" \]/,/^fi$/d'
run "$m" -- --print-sign-plan "$work/b-team"
expect_red "P2 over a copy without the team + ad hoc rule" 1 'ad hoc'

mutant envteam 's/\[ -n "\${LIBRESCRS_TEAM_ID+set}" \]/false/'
run "$m" LIBRESCRS_TEAM_ID=ZZZZZ99999 CODESIGN_IDENTITY="Fake Identity" -- --print-sign-plan "$work/b-team"
expect_red "P5 over a copy without the environment comparison" 1 'in the environment'

mutant postsign 's/if \[ "\$signed_team" != "\$team" \]; then/if false; then/'
run "$m" CODESIGN_IDENTITY="Fake Identity" FAKE_TEAM=XYZXYZ0000 -- --sign-bundle "$work/b-team" "$work/app"
expect_red "S4 over a copy without the check after signing" 1 "signed by team 'XYZXYZ0000'"

mutant dlv 's/if \[ "\$dlv" = no \]; then/if false; then/'
run "$m" CODESIGN_IDENTITY="Fake Identity" FAKE_TEAM=ABCDE12345 -- --sign-bundle "$work/b-team" "$work/app"
expect_red "S2 over a copy that keeps disable-library-validation for a team" 0 'log:!disable-library-validation'

mutant timestamp 's/^    ts=--timestamp=none$/    ts=--timestamp/'
run "$m" -- --sign-bundle "$work/b-none" "$work/app"
expect_red "S1 over a copy that timestamps an ad hoc signature" 0 'log:--timestamp=none'

echo
if [ "$failed" -ne 0 ] || [ "$red" -ne "$red_expected" ] || [ "$red" -eq 0 ]; then
    echo "selftest: $cases cases, $red red-proved, $failed FAILED"
    exit 1
fi
echo "selftest: $cases cases, $red red-proved"
