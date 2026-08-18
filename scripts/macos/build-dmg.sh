#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a LibreCelik DMG for macOS.
#
# Usage:  ./scripts/macos/build-dmg.sh [BUILD_DIR]
#   BUILD_DIR  CMake build directory (default: build)
#
# Prerequisites:
#   - CMake Release build already compiled in BUILD_DIR
#   - macdeployqt on PATH  OR  Qt installed under ~/Qt/<version>/macos/bin/
#
# The script:
#   1. Copies the built .app to a staging area
#   2. Runs macdeployqt to bundle Qt frameworks and plugins
#   3. Ad-hoc signs the bundle (no Apple Developer account needed;
#      users can bypass Gatekeeper with right-click → Open)
#   4. Creates a compressed DMG containing the .app and a /Applications symlink
#
# Output: LibreCelik-<VERSION>-macos.dmg in the project root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"

# ---------------------------------------------------------------------------
# Resolve macdeployqt
# ---------------------------------------------------------------------------
MACDEPLOYQT=""

# 1. Check PATH first
if command -v macdeployqt &>/dev/null; then
    MACDEPLOYQT="$(command -v macdeployqt)"
fi

# 2. Search ~/Qt/<version>/macos/bin/, newest version first
if [[ -z "$MACDEPLOYQT" ]]; then
    while IFS= read -r candidate; do
        if [[ -x "$candidate" ]]; then
            MACDEPLOYQT="$candidate"
            break
        fi
    done < <(ls -d "$HOME"/Qt/*/macos/bin/macdeployqt 2>/dev/null | sort -Vr)
fi

if [[ -z "$MACDEPLOYQT" ]]; then
    echo "ERROR: macdeployqt not found."
    echo "       Install Qt via the Qt installer or add Qt's bin/ to PATH."
    exit 1
fi
echo "macdeployqt: $MACDEPLOYQT"

# ---------------------------------------------------------------------------
# Determine version from git tag
# ---------------------------------------------------------------------------
# --abbrev=0 is deliberate HERE and nowhere else: the artefact file name wants
# the release number, not a describe string with a commit hash in it. --match
# keeps a local rollback/bookkeeping tag from ever becoming a "version" — the
# same restriction cmake/GitVersion.cmake applies.
VERSION=$(git -C "$PROJECT_ROOT" describe --tags --abbrev=0 --match "[0-9]*" --match "v[0-9]*" 2>/dev/null || echo "dev")
echo "Building DMG for version: $VERSION"

# ---------------------------------------------------------------------------
# Locate the built .app
# ---------------------------------------------------------------------------
APP_SRC="$BUILD_DIR/src/LibreCelik.app"
if [[ ! -d "$APP_SRC" ]]; then
    echo "ERROR: App bundle not found at $APP_SRC"
    echo "       Run: cmake --build $BUILD_DIR first"
    exit 1
fi

# ---------------------------------------------------------------------------
# Stage a working copy of the .app (macdeployqt modifies it in-place)
# ---------------------------------------------------------------------------
STAGING_DIR="$(mktemp -d)/LibreCelik-dmg-staging"
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
# which this bundle never ships (the app guides the user to install it), so
# there is no card-side plugin directory, PKCS#11 module or certificate store
# to stage here.
# ---------------------------------------------------------------------------
GUI_PLUGIN_DIR="$BUILD_DIR/gui-plugins"

# --- DSS signing bundle (JRE + JAR) ---
# Skipped: native signing backend is the default and does not require Java.
# DSS is deprecated. To re-enable, set SIGNING_BACKEND=dss and uncomment.

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
# Verify every bundled library has a documented license. Runs after the bundle
# is fully populated (macdeployqt done) and before signing seals it.
#
# Bootstrap gate: the manifest currently maps Linux sonames only. Until the
# macOS .dylib entries (platforms:["macos"]) are authored and the manifest's
# "macos_bootstrapped" flag is flipped to true, a fail-closed check would block
# every macOS release. So while not bootstrapped we only EMIT the candidate
# dylib list as a non-fatal informational warning; once bootstrapped we enforce
# fail-closed exactly like the Linux AppImage path.
# ---------------------------------------------------------------------------
LICENSE_CHECKER="$PROJECT_ROOT/ci/scripts/check-bundled-licenses.py"
LICENSE_MANIFEST="$PROJECT_ROOT/licenses/manifest.json"
MACOS_BOOTSTRAPPED="$(python3 -c "import json,sys; print('true' if json.load(open(sys.argv[1])).get('macos_bootstrapped') else 'false')" "$LICENSE_MANIFEST")"

if [[ "$MACOS_BOOTSTRAPPED" == "true" ]]; then
    echo "Verifying bundled-license completeness..."
    python3 "$LICENSE_CHECKER" \
        --check "$APP_STAGING" \
        --manifest "$LICENSE_MANIFEST" \
        --platform macos || {
            echo "ERROR: bundled-license check failed — a bundled library lacks a documented license (see ::error:: lines above)." >&2
            exit 1
        }
else
    echo "::warning::macOS license manifest not yet bootstrapped — author these entries (platforms:[\"macos\"]) and set macos_bootstrapped=true to enforce."
    echo "::warning::Candidate dylibs found in the app bundle:"
    python3 "$LICENSE_CHECKER" \
        --emit-candidates "$APP_STAGING" \
        --manifest "$LICENSE_MANIFEST" \
        --platform macos || true
fi

# ---------------------------------------------------------------------------
# Ad-hoc sign — satisfies macOS's basic integrity check without an Apple ID.
# Users on non-developer machines will still see a Gatekeeper warning on first
# launch; they can bypass it with right-click → Open.
# ---------------------------------------------------------------------------
echo "Ad-hoc signing..."
ENTITLEMENTS="$PROJECT_ROOT/resources/macos/LibreCelik.entitlements"
codesign --deep --force --sign - --entitlements "$ENTITLEMENTS" --options runtime "$APP_STAGING"

# ---------------------------------------------------------------------------
# Build the DMG
# ---------------------------------------------------------------------------
DMG_STAGING="$(mktemp -d)/LibreCelik-dmg-contents"
mkdir -p "$DMG_STAGING"

echo "Assembling DMG contents..."
cp -R "$APP_STAGING" "$DMG_STAGING/"
ln -s /Applications "$DMG_STAGING/Applications"

OUTPUT="$PROJECT_ROOT/LibreCelik-$VERSION-macos.dmg"

echo "Creating DMG..."
hdiutil create \
    -volname "LibreCelik $VERSION" \
    -srcfolder "$DMG_STAGING" \
    -ov \
    -format UDZO \
    "$OUTPUT"

echo ""
echo "DMG created: $OUTPUT"
