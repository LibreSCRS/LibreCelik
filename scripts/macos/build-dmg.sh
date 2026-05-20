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
VERSION=$(git -C "$PROJECT_ROOT" describe --tags --abbrev=0 2>/dev/null || echo "dev")
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
# Copy LibreSCRS plugins (middleware + GUI) into the app bundle.
#
# The app resolves these at runtime via ../PlugIns/ relative to the binary
# (i.e. Contents/PlugIns/).
# ---------------------------------------------------------------------------
MW_PLUGIN_DIR="$BUILD_DIR/plugins"
GUI_PLUGIN_DIR="$BUILD_DIR/gui-plugins"

# --- PKCS#11 module (for digital signing via PKCS#11 tokens) ---
# lib/pkcs11/CMakeLists.txt forces SUFFIX ".so" on every POSIX (including
# macOS), so the file is librescrs-pkcs11.so (unversioned symlink pointing
# to librescrs-pkcs11.<VERSION>.so on macOS, librescrs-pkcs11.so.<VERSION>
# on Linux). cp follows the symlink and copies the real versioned file in.
PKCS11_LIB="$BUILD_DIR/lib/pkcs11/librescrs-pkcs11.so"
if [[ -e "$PKCS11_LIB" ]]; then
    echo "Copying PKCS#11 module..."
    # Frameworks/ is created later by macdeployqt; pre-create so we can stage here first.
    mkdir -p "$APP_STAGING/Contents/Frameworks"
    cp "$PKCS11_LIB" "$APP_STAGING/Contents/Frameworks/librescrs-pkcs11.so"
    echo "  librescrs-pkcs11.so"
else
    echo "ERROR: PKCS#11 module not found at $PKCS11_LIB — signing would not work."
    echo "       Ensure LibreMiddleware builds librescrs-pkcs11 with LIBRARY_OUTPUT_DIRECTORY set to \${CMAKE_BINARY_DIR}/lib/pkcs11."
    exit 1
fi

# --- DSS signing bundle (JRE + JAR) ---
# Skipped: native signing backend is the default and does not require Java.
# DSS is deprecated. To re-enable, set SIGNING_BACKEND=dss and uncomment.

echo "Copying middleware plugins..."
mkdir -p "$APP_STAGING/Contents/PlugIns/middleware-plugins"
for f in "$MW_PLUGIN_DIR"/lib*-plugin.dylib; do
    [[ -f "$f" ]] && cp "$f" "$APP_STAGING/Contents/PlugIns/middleware-plugins/"
done
ls "$APP_STAGING/Contents/PlugIns/middleware-plugins/"

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
