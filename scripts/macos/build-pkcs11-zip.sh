#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a LibreSCRS PKCS#11 module ZIP for macOS.
#
# Usage:  ./scripts/macos/build-pkcs11-zip.sh [BUILD_DIR]
#   BUILD_DIR  CMake build directory (default: build)
#
# Prerequisites:
#   - CMake Release build already compiled in BUILD_DIR
#   - LibreMiddleware source available (FetchContent _deps or local clone)
#
# The ZIP contains:
#   lib/    librescrs-pkcs11.<version>.dylib  (universal arm64+x86_64)
#           librescrs-pkcs11.1.dylib          (soname symlink)
#           librescrs-pkcs11.dylib            (unversioned symlink)
#   include/pkcs11/pkcs11.h, pkcs11t.h, pkcs11f.h
#           pkcs11_version.h
#   README.txt
#
# Output: LibreSCRS-pkcs11-<VERSION>-macos.zip in the project root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"

# ---------------------------------------------------------------------------
# LibreMiddleware version — read from the project's CMakeLists.txt.
# This is the declared release version, independent of any local dev clone.
# ---------------------------------------------------------------------------
VERSION=$(grep -m1 "^set(MIDDLEWARE_VERSION " "$PROJECT_ROOT/CMakeLists.txt" \
          | sed 's/set(MIDDLEWARE_VERSION[[:space:]]*\(.*\))/\1/' | tr -d ')')

if [[ -z "$VERSION" ]]; then
    echo "ERROR: Could not read MIDDLEWARE_VERSION from $PROJECT_ROOT/CMakeLists.txt"
    exit 1
fi
echo "LibreMiddleware version: $VERSION"

# ---------------------------------------------------------------------------
# Locate the built dylib.
# The versioned file is the only non-symlink dylib in the lib dir:
#   librescrs-pkcs11.<major>.<minor>.<patch>.dylib  (real file)
#   librescrs-pkcs11.<major>.dylib                  (soname symlink)
#   librescrs-pkcs11.dylib                          (unversioned symlink)
# ---------------------------------------------------------------------------
LIB_DIR="$BUILD_DIR/_deps/libremiddleware-build/lib/pkcs11"

if [[ ! -d "$LIB_DIR" ]]; then
    echo "ERROR: PKCS#11 build directory not found at $LIB_DIR"
    echo "       Run: cmake --build $BUILD_DIR first"
    exit 1
fi

# Find the real (non-symlink) versioned dylib
DYLIB_VERSIONED=$(find "$LIB_DIR" -maxdepth 1 -name "librescrs-pkcs11.[0-9]*.dylib" ! -type l | sort -V | tail -1)

if [[ -z "$DYLIB_VERSIONED" ]]; then
    echo "ERROR: No versioned dylib found in $LIB_DIR"
    echo "       Run: cmake --build $BUILD_DIR first"
    exit 1
fi

BUILT_VERSION=$(basename "$DYLIB_VERSIONED" | sed 's/librescrs-pkcs11\.\(.*\)\.dylib/\1/')
if [[ "$BUILT_VERSION" != "$VERSION" ]]; then
    echo "WARNING: Built dylib version ($BUILT_VERSION) differs from declared MIDDLEWARE_VERSION ($VERSION)."
    echo "         Packaging as $VERSION — rebuild against the released middleware tag if needed."
fi

echo "Building PKCS#11 ZIP for LibreMiddleware version: $VERSION"

# ---------------------------------------------------------------------------
# Locate LibreMiddleware source (for public PKCS#11 headers)
# ---------------------------------------------------------------------------
MIDDLEWARE_SRC=""

# 1. Read from CMake cache (set when FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE is used)
CMAKE_CACHE="$BUILD_DIR/CMakeCache.txt"
if [[ -f "$CMAKE_CACHE" ]]; then
    cached=$(grep -m1 "^FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE:PATH=" "$CMAKE_CACHE" | cut -d= -f2-)
    if [[ -n "$cached" && -f "$cached/lib/pkcs11/include/pkcs11/pkcs11.h" ]]; then
        MIDDLEWARE_SRC="$cached"
    fi
fi

# 2. Standard FetchContent source dir
if [[ -z "$MIDDLEWARE_SRC" && -f "$BUILD_DIR/_deps/libremiddleware-src/lib/pkcs11/include/pkcs11/pkcs11.h" ]]; then
    MIDDLEWARE_SRC="$BUILD_DIR/_deps/libremiddleware-src"
fi

# 3. Sibling directory (common local development layout)
if [[ -z "$MIDDLEWARE_SRC" ]]; then
    sibling="$(dirname "$PROJECT_ROOT")/LibreMiddleware"
    if [[ -f "$sibling/lib/pkcs11/include/pkcs11/pkcs11.h" ]]; then
        MIDDLEWARE_SRC="$sibling"
    fi
fi

if [[ -z "$MIDDLEWARE_SRC" ]]; then
    echo "ERROR: Cannot locate LibreMiddleware source (needed for PKCS#11 headers)."
    echo "       Build with FetchContent (creates _deps/libremiddleware-src) or"
    echo "       set FETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE in your CMake cache."
    exit 1
fi
echo "Middleware source: $MIDDLEWARE_SRC"

PKCS11_HEADERS_DIR="$MIDDLEWARE_SRC/lib/pkcs11/include/pkcs11"
PKCS11_VERSION_H="$LIB_DIR/pkcs11_version.h"

# ---------------------------------------------------------------------------
# Stage package layout
# ---------------------------------------------------------------------------
STAGING_PARENT="$(mktemp -d)"
PKG_NAME="librescrs-pkcs11-$VERSION-macos"
STAGING="$STAGING_PARENT/$PKG_NAME"

mkdir -p "$STAGING/lib" "$STAGING/include/pkcs11"

echo "Staging library files..."
# Copy the built dylib under the declared VERSION name (the internal soname
# @rpath/librescrs-pkcs11.1.dylib is based on SOVERSION and is unaffected).
cp "$DYLIB_VERSIONED" "$STAGING/lib/librescrs-pkcs11.$VERSION.dylib"
# Recreate the soname and unversioned symlinks
(
    cd "$STAGING/lib"
    ln -sf "librescrs-pkcs11.$VERSION.dylib" "librescrs-pkcs11.1.dylib"
    ln -sf "librescrs-pkcs11.1.dylib"        "librescrs-pkcs11.dylib"
)

# ---------------------------------------------------------------------------
# Ad-hoc sign — no Apple Developer account needed; users bypass Gatekeeper
# with right-click → Open on first use of the library
# ---------------------------------------------------------------------------
echo "Ad-hoc signing..."
codesign --force --sign - "$STAGING/lib/librescrs-pkcs11.$VERSION.dylib"

# ---------------------------------------------------------------------------
# Copy headers
# ---------------------------------------------------------------------------
echo "Copying headers..."
cp "$PKCS11_HEADERS_DIR/pkcs11.h"  "$STAGING/include/pkcs11/"
cp "$PKCS11_HEADERS_DIR/pkcs11t.h" "$STAGING/include/pkcs11/"
cp "$PKCS11_HEADERS_DIR/pkcs11f.h" "$STAGING/include/pkcs11/"
cp "$PKCS11_VERSION_H"             "$STAGING/include/"

# ---------------------------------------------------------------------------
# README
# ---------------------------------------------------------------------------
cat > "$STAGING/README.txt" << EOF
LibreSCRS PKCS#11 Module for macOS — version $VERSION
======================================================

CONTENTS
  lib/librescrs-pkcs11.$VERSION.dylib   Universal (arm64 + x86_64) PKCS#11 module
  lib/librescrs-pkcs11.1.dylib          Soname symlink
  lib/librescrs-pkcs11.dylib            Unversioned symlink
  include/pkcs11/                        Standard OASIS PKCS#11 v3.x headers
  include/pkcs11_version.h               LibreSCRS version macros

INSTALLATION
  Copy the lib/ contents to a directory of your choice, e.g.:
    sudo cp -R lib/ /usr/local/lib/

  To register the module with a PKCS#11-aware application, point it at the
  full path of librescrs-pkcs11.dylib (or the versioned file).

  Example — Firefox (about:config → security.osclientcerts.autoload or
  security.pkcs11.modulecanregister in about:config, then use the
  pkcs11-tool from OpenSC to load the module):
    pkcs11-tool --module /usr/local/lib/librescrs-pkcs11.dylib --list-slots

DEPENDENCIES
  The module links only against system frameworks:
    PCSC.framework   (built-in macOS smart card subsystem)
    libz, libc++, libSystem

GATEKEEPER NOTE
  This build is ad-hoc signed (no Apple notarisation).  On first use
  macOS may warn "unidentified developer".  To allow it:
    System Settings → Privacy & Security → scroll down → "Allow Anyway"

LICENSE
  LGPL-2.1-or-later  — see https://github.com/LibreSCRS/LibreMiddleware
EOF

# ---------------------------------------------------------------------------
# Package into ZIP (preserving symlinks with -y)
# ---------------------------------------------------------------------------
OUTPUT="$PROJECT_ROOT/LibreSCRS-pkcs11-$VERSION-macos.zip"
echo "Creating ZIP..."
(cd "$STAGING_PARENT" && zip -r -y "$OUTPUT" "$PKG_NAME")

echo ""
echo "ZIP created: $OUTPUT"
