#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Package and sign LibreCelik.app for macOS. No disk image: the macOS release
# ships this bundle inside LibreMac's disk image, beside LibreMac.app, and
# LibreMac's release builds it from this repository's revision.
#
# Usage:  scripts/macos/build-app.sh <BUILD_DIR> <OUT_DIR>
#         scripts/macos/build-app.sh --print-sign-plan <BUILD_DIR>
#         scripts/macos/build-app.sh --sign-bundle <BUILD_DIR> <APP>
#
#   BUILD_DIR  configured and built CMake Release tree
#   OUT_DIR    receives LibreCelik.app and sbom-librecelik-macos.cdx.json
#
# --print-sign-plan prints the signing plan (identity, timestamp, library
# validation) or refuses, and signs nothing. --sign-bundle signs an already
# staged bundle in place -- the second half of the packaging path, on its own.
#
# Environment:
#   CODESIGN_IDENTITY  signing identity; default "-" (ad hoc)
#   LIBRESCRS_TEAM_ID  optional; when set it must equal the team the build was
#                      configured with
#   MACDEPLOYQT        optional; path to macdeployqt
#
# Exit 0 on success, 1 on a refusal or an error. Bash 3.2 compatible: the
# macOS runners and a stock Mac have no newer bash.
#
# Identity rules mirror LibreMac's Scripts/sign-bundle.sh: two products, each
# signs its own bundle, by the same rules.
#
# The team is read from BUILD_DIR/CMakeCache.txt (LIBRESCRS_TEAM_ID), because
# the agent client library bakes it in: the App Group that holds the agent's
# socket is named after it, and the client holds the agent to that team's
# signature. The identity signing here must be that team: ad hoc with a team
# named ships a client that cannot share a container with an agent signed by
# that team, and a team identity with no team named ships a client that never
# checks who signed the agent.
#
# Every signature carries the hardened runtime. An ad hoc signature carries no
# timestamp; any other identity gets Apple's secure timestamp, which
# notarization requires (network access to Apple's timestamp server).
#
# Library validation: the hardened runtime makes dyld accept only libraries
# signed by the executable's own team. An ad hoc signature has no team, so the
# app gets com.apple.security.cs.disable-library-validation exactly when the
# build names no team. A team-signed bundle validates and does not get it.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
ENTITLEMENTS_SRC="$PROJECT_ROOT/resources/macos/LibreCelik.entitlements"

die() { echo "build-app: $*" >&2; exit 1; }
usage() {
    echo "usage: build-app.sh <BUILD_DIR> <OUT_DIR>" >&2
    echo "       build-app.sh --print-sign-plan <BUILD_DIR>" >&2
    echo "       build-app.sh --sign-bundle <BUILD_DIR> <APP>" >&2
    exit 1
}

mode=package
case "${1:-}" in
    --print-sign-plan) mode=plan; shift; [ $# -eq 1 ] || usage ;;
    --sign-bundle) mode=sign; shift; [ $# -eq 2 ] || usage ;;
    -*) usage ;;
    *) [ $# -eq 2 ] || usage ;;
esac
BUILD_DIR="$1"
CODESIGN_IDENTITY="${CODESIGN_IDENTITY:--}"

# ---------------------------------------------------------------------------
# Signing plan: team, identity, timestamp, library validation. Decided before
# anything is staged, so a refusal costs nothing.
# ---------------------------------------------------------------------------
cache="$BUILD_DIR/CMakeCache.txt"
[ -f "$cache" ] || die "no CMakeCache.txt in $BUILD_DIR -- configure and build LibreCelik there first"
team_line="$(grep '^LIBRESCRS_TEAM_ID:' "$cache" || true)"
[ -n "$team_line" ] || die "$cache carries no LIBRESCRS_TEAM_ID -- the agent client library was not configured in this build"
team="${team_line#*=}"
if [ -n "${LIBRESCRS_TEAM_ID+set}" ] && [ "$LIBRESCRS_TEAM_ID" != "$team" ]; then
    die "LIBRESCRS_TEAM_ID is '$LIBRESCRS_TEAM_ID' in the environment but the build was configured with '$team' -- the client carries the configured one; reconfigure with -DLIBRESCRS_TEAM_ID=$LIBRESCRS_TEAM_ID or unset it"
fi
if [ "$CODESIGN_IDENTITY" = "-" ] && [ -n "$team" ]; then
    die "the build names team '$team' but CODESIGN_IDENTITY is ad hoc: the client would require that team's signature on an agent while this bundle has none -- set CODESIGN_IDENTITY to that team's identity, or reconfigure with an empty LIBRESCRS_TEAM_ID"
fi
if [ "$CODESIGN_IDENTITY" = "-" ]; then timestamp=no; else timestamp=yes; fi
if [ -z "$team" ]; then dlv=yes; else dlv=no; fi

if [ "$mode" = plan ]; then
    echo "identity=$CODESIGN_IDENTITY"
    echo "team=$team"
    echo "timestamp=$timestamp"
    echo "dlv=$dlv"
    exit 0
fi

# sign_bundle <APP>: inside out, never --deep, then the post-signing checks.
sign_bundle() {
    local app="$1" work ts f fw signed_team consequence id_label
    [ -x "$app/Contents/MacOS/LibreCelik" ] || die "no executable at $app/Contents/MacOS/LibreCelik"

    # The entitlements are signed from a copy, never from the source file.
    work="$(mktemp -d)"
    cp "$ENTITLEMENTS_SRC" "$work/LibreCelik.entitlements"
    if [ "$dlv" = no ]; then
        /usr/libexec/PlistBuddy -c "Delete :com.apple.security.cs.disable-library-validation" "$work/LibreCelik.entitlements" >/dev/null
    else
        echo "build-app: no team named -- the app carries disable-library-validation"
    fi

    ts=--timestamp=none
    [ "$timestamp" = no ] || ts=--timestamp
    sign() { codesign --force -s "$CODESIGN_IDENTITY" --options runtime "$ts" "$@"; }

    # 1) every Mach-O outside a framework bundle: dylibs, Qt plugins, GUI plugins
    while IFS= read -r -d '' f; do
        sign "$f"
    done < <(find "$app/Contents" -type f \( -name '*.dylib' -o -name '*.so' \) ! -path '*.framework/*' -print0)
    # 2) every framework, through its version directory (not the symlink)
    while IFS= read -r -d '' fw; do
        sign "$fw/Versions/A"
    done < <(find "$app/Contents/Frameworks" -maxdepth 1 -name '*.framework' -print0)
    # 3) the main executable and the bundle, last; entitlements only here
    sign --entitlements "$work/LibreCelik.entitlements" "$app"
    rm -rf "$work"

    codesign --verify --strict --deep "$app" || die "codesign --verify --strict --deep refused $app"

    if [ "$CODESIGN_IDENTITY" != "-" ]; then
        signed_team="$(codesign -dv "$app" 2>&1 | sed -n 's/^TeamIdentifier=//p')"
        if [ "$signed_team" != "$team" ]; then
            consequence="require a signature from team '$team' that its own bundle does not carry"
            [ -n "$team" ] || consequence="never check who signed the agent"
            die "the bundle was signed by team '$signed_team' but the build names '$team': the client would $consequence -- reconfigure with -DLIBRESCRS_TEAM_ID set to the signing team and rebuild"
        fi
    fi

    id_label="ad hoc"
    [ "$CODESIGN_IDENTITY" = "-" ] || id_label="identity=$CODESIGN_IDENTITY"
    echo "build-app: signed $app ($id_label, inside out, hardened runtime)"
}

if [ "$mode" = sign ]; then
    sign_bundle "$2"
    exit 0
fi

OUT_DIR="$2"

# ---------------------------------------------------------------------------
# Resolve macdeployqt: MACDEPLOYQT, then PATH, then ~/Qt/<version>/macos/bin/
# (newest version first).
# ---------------------------------------------------------------------------
if [[ -z "${MACDEPLOYQT:-}" ]] && command -v macdeployqt &>/dev/null; then
    MACDEPLOYQT="$(command -v macdeployqt)"
fi
if [[ -z "${MACDEPLOYQT:-}" ]]; then
    while IFS= read -r candidate; do
        if [[ -x "$candidate" ]]; then
            MACDEPLOYQT="$candidate"
            break
        fi
    done < <(ls -d "$HOME"/Qt/*/macos/bin/macdeployqt 2>/dev/null | sort -Vr)
fi
if [[ -z "${MACDEPLOYQT:-}" ]]; then
    die "macdeployqt not found -- install Qt via the Qt installer, add Qt's bin/ to PATH, or set MACDEPLOYQT"
fi
echo "macdeployqt: $MACDEPLOYQT"

# ---------------------------------------------------------------------------
# Version, for the bill of materials. The resolution is shared with the other
# packaging script and mirrors cmake/GitVersion.cmake — it refuses a foreign
# repository's tags, accepts only version-shaped ones, and falls back to the
# VERSION file.
# ---------------------------------------------------------------------------
# shellcheck source=../project-version.sh
. "$SCRIPT_DIR/../project-version.sh"
VERSION="$(project_version "$PROJECT_ROOT")"
echo "Packaging LibreCelik.app for version: $VERSION"

# ---------------------------------------------------------------------------
# Locate the built .app
# ---------------------------------------------------------------------------
APP_SRC="$BUILD_DIR/src/LibreCelik.app"
[[ -d "$APP_SRC" ]] || die "app bundle not found at $APP_SRC -- run: cmake --build $BUILD_DIR first"

# ---------------------------------------------------------------------------
# Stage a working copy of the .app (macdeployqt modifies it in-place)
# ---------------------------------------------------------------------------
STAGING_ROOT="$(mktemp -d)"
trap 'rm -rf "$STAGING_ROOT"' EXIT
STAGING_DIR="$STAGING_ROOT/LibreCelik-app-staging"
APP_STAGING="$STAGING_DIR/LibreCelik.app"

echo "Staging .app..."
mkdir -p "$STAGING_DIR"
cp -R "$APP_SRC" "$APP_STAGING"

# ---------------------------------------------------------------------------
# Copy the LibreSCRS GUI plugins into the app bundle.
#
# The app resolves these at runtime via ../PlugIns/ relative to the binary
# (i.e. Contents/PlugIns/).
#
# Card access is not bundled at all: it lives in the separate agent service,
# which this bundle never ships (LibreMac carries it), so there is no
# card-side plugin directory, PKCS#11 module or certificate store to stage
# here.
# ---------------------------------------------------------------------------
GUI_PLUGIN_DIR="$BUILD_DIR/gui-plugins"

echo "Copying GUI plugins..."
mkdir -p "$APP_STAGING/Contents/PlugIns/gui-plugins"
for f in "$GUI_PLUGIN_DIR"/*-gui-plugin.*; do
    [[ -f "$f" ]] && cp "$f" "$APP_STAGING/Contents/PlugIns/gui-plugins/"
done
ls "$APP_STAGING/Contents/PlugIns/gui-plugins/"

# ---------------------------------------------------------------------------
# Run macdeployqt — bundles Qt frameworks, plugins, and .qm translations
# ---------------------------------------------------------------------------
echo "Running macdeployqt..."
"$MACDEPLOYQT" "$APP_STAGING" -verbose=1

# ---------------------------------------------------------------------------
# Agent client library.
#
# The executable links it, so macdeployqt stages it into Contents/Frameworks
# alongside the Qt frameworks and rewrites its install name. The wire codec is
# a static archive folded into this library, so there is no second dynamic
# library to look for. Assert it landed: without it the app cannot reach the
# agent at all, and a silent miss would only surface on the user's machine.
# ---------------------------------------------------------------------------
if ! ls "$APP_STAGING/Contents/Frameworks"/liblibrescrs-agentclient-qt*.dylib &>/dev/null; then
    echo "ERROR: agent client library not found in $APP_STAGING/Contents/Frameworks —"
    echo "       macdeployqt did not stage liblibrescrs-agentclient-qt*.dylib."
    echo "       Check that the executable links it and that its build-tree"
    echo "       install name resolves."
    exit 1
fi
echo "Agent client library in bundle:"
ls "$APP_STAGING/Contents/Frameworks"/liblibrescrs-agentclient-qt*.dylib

# ---------------------------------------------------------------------------
# Verify every bundled library has a documented license, fail-closed, after the
# bundle is fully populated and before signing seals it.
# ---------------------------------------------------------------------------
LICENSE_CHECKER="$PROJECT_ROOT/ci/scripts/check-bundled-licenses.py"
LICENSE_MANIFEST="$PROJECT_ROOT/licenses/manifest.json"

echo "Verifying bundled-license completeness..."
python3 "$LICENSE_CHECKER" \
    --check "$APP_STAGING" \
    --manifest "$LICENSE_MANIFEST" \
    --platform macos || {
        echo "ERROR: bundled-license check failed — a bundled library lacks a documented license (see ::error:: lines above)." >&2
        exit 1
    }

# ---------------------------------------------------------------------------
# Sign, inside out.
# ---------------------------------------------------------------------------
sign_bundle "$APP_STAGING"

# ---------------------------------------------------------------------------
# Bill of materials for what this bundle actually carries, derived from the
# staging tree and not from source pins: a source-pin reader once searched for
# files this repository does not have, printed "0 components", exited 0, and
# the empty document was signed and published beside the artefact.
#
# AFTER signing, and the order is the whole point of the hash: codesign
# rewrites the bytes of every Mach-O it signs. A bill written before that step
# would publish a sha256 for a file that no longer exists in the bundle, which
# is worse than publishing none: the reader who checks it concludes the
# artefact was tampered with. Nothing may touch the bundle afterwards.
#
# The bill refuses an unmapped library itself rather than shipping a shorter
# bill than the bundle.
# ---------------------------------------------------------------------------
mkdir -p "$OUT_DIR"
echo "Writing the bill of materials..."
SBOM_OUT="$OUT_DIR/sbom-librecelik-macos.cdx.json"
python3 "$LICENSE_CHECKER" \
    --sbom "$APP_STAGING" \
    --sbom-out "$SBOM_OUT" \
    --manifest "$LICENSE_MANIFEST" \
    --app-name LibreCelik \
    --app-version "$VERSION" \
    --platform macos || {
        echo "ERROR: bill of materials refused (see ::error:: lines above) — not packaging." >&2
        exit 1
    }

rm -rf "$OUT_DIR/LibreCelik.app"
ditto "$APP_STAGING" "$OUT_DIR/LibreCelik.app"

echo ""
echo "App bundle: $OUT_DIR/LibreCelik.app"
echo "Bill of materials: $SBOM_OUT"
