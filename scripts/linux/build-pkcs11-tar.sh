#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a LibreSCRS PKCS#11 module tarball for Linux.
#
# Usage:  ./scripts/linux/build-pkcs11-tar.sh [BUILD_DIR]
#   BUILD_DIR  CMake build directory (default: build)
#
# Prerequisites:
#   - CMake Release build already compiled in BUILD_DIR
#   - LibreMiddleware source available (FetchContent _deps or local clone)
#
# The tarball contains:
#   lib/    librescrs-pkcs11.so.<version>  (the shared library)
#           librescrs-pkcs11.so.1          (soname symlink)
#           librescrs-pkcs11.so            (unversioned symlink)
#   include/pkcs11/pkcs11.h, pkcs11t.h, pkcs11f.h
#           pkcs11_version.h
#   README.txt
#
# Output: LibreSCRS-pkcs11-<VERSION>-linux-x86_64.tar.gz in the project root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"

# ---------------------------------------------------------------------------
# LibreMiddleware version — read from the project's CMakeLists.txt.
# This is the declared release version, independent of any local dev clone.
# ---------------------------------------------------------------------------
ARCH=$(uname -m)

VERSION=$(grep -m1 "^set(MIDDLEWARE_VERSION " "$PROJECT_ROOT/CMakeLists.txt" \
          | sed 's/set(MIDDLEWARE_VERSION[[:space:]]*\(.*\))/\1/' | tr -d ')')

if [[ -z "$VERSION" ]]; then
    echo "ERROR: Could not read MIDDLEWARE_VERSION from $PROJECT_ROOT/CMakeLists.txt"
    exit 1
fi
echo "LibreMiddleware version: $VERSION"

# ---------------------------------------------------------------------------
# Locate the built shared library.
# The versioned file is the only non-symlink .so in the lib dir:
#   librescrs-pkcs11.so.<major>.<minor>.<patch>  (real file)
#   librescrs-pkcs11.so.<major>                  (soname symlink)
#   librescrs-pkcs11.so                          (unversioned symlink)
# ---------------------------------------------------------------------------
LIB_DIR="$BUILD_DIR/_deps/libremiddleware-build/lib/pkcs11"

if [[ ! -d "$LIB_DIR" ]]; then
    echo "ERROR: PKCS#11 build directory not found at $LIB_DIR"
    echo "       Run: cmake --build $BUILD_DIR first"
    exit 1
fi

# Find the real (non-symlink) versioned .so (has at least two dots after .so)
SO_VERSIONED=$(find "$LIB_DIR" -maxdepth 1 -name "librescrs-pkcs11.so.[0-9]*.[0-9]*" ! -type l | sort -V | tail -1)

if [[ -z "$SO_VERSIONED" ]]; then
    echo "ERROR: No versioned .so found in $LIB_DIR"
    echo "       Run: cmake --build $BUILD_DIR first"
    exit 1
fi

BUILT_VERSION=$(basename "$SO_VERSIONED" | sed 's/librescrs-pkcs11\.so\.\(.*\)/\1/')
if [[ "$BUILT_VERSION" != "$VERSION" ]]; then
    echo "WARNING: Built .so version ($BUILT_VERSION) differs from declared MIDDLEWARE_VERSION ($VERSION)."
    echo "         Packaging as $VERSION — rebuild against the released middleware tag if needed."
fi

echo "Building PKCS#11 tarball for LibreMiddleware version: $VERSION ($ARCH)"

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
PKG_NAME="librescrs-pkcs11-$VERSION-linux-$ARCH"
STAGING="$STAGING_PARENT/$PKG_NAME"

mkdir -p "$STAGING/lib" "$STAGING/include/pkcs11"

echo "Staging library files..."
# Copy the built .so under the declared VERSION name (the internal soname
# librescrs-pkcs11.so.1 is based on SOVERSION and is unaffected).
cp "$SO_VERSIONED" "$STAGING/lib/librescrs-pkcs11.so.$VERSION"
# Recreate soname and unversioned symlinks
(
    cd "$STAGING/lib"
    ln -sf "librescrs-pkcs11.so.$VERSION" "librescrs-pkcs11.so.1"
    ln -sf "librescrs-pkcs11.so.1"        "librescrs-pkcs11.so"
)

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
LibreSCRS PKCS#11 Module for Linux — version $VERSION ($ARCH)
==============================================================

CONTENTS
  lib/librescrs-pkcs11.so.$VERSION   The shared PKCS#11 module
  lib/librescrs-pkcs11.so.1          Soname symlink
  lib/librescrs-pkcs11.so            Unversioned symlink
  include/pkcs11/                     Standard OASIS PKCS#11 v3.x headers
  include/pkcs11_version.h            LibreSCRS version macros

INSTALLATION
  Copy the lib/ contents to a directory on the library path, e.g.:
    sudo cp lib/librescrs-pkcs11.so.$VERSION /usr/local/lib/
    sudo ln -sf librescrs-pkcs11.so.$VERSION /usr/local/lib/librescrs-pkcs11.so.1
    sudo ln -sf librescrs-pkcs11.so.1        /usr/local/lib/librescrs-pkcs11.so
    sudo ldconfig

  To register the module with a PKCS#11-aware application, point it at the
  full path of librescrs-pkcs11.so (or the versioned file).

  Example — verify with OpenSC tools:
    pkcs11-tool --module /usr/local/lib/librescrs-pkcs11.so --list-slots

DEPENDENCIES
  The module links against:
    libpcsclite.so   (PC/SC lite smart card daemon client)
    libz, libstdc++, libm

  Ensure pcscd is installed and running:
    sudo apt install pcscd libpcsclite1   # Debian/Ubuntu
    sudo dnf install pcsc-lite           # Fedora/RHEL

LICENSE
  LGPL-2.1-or-later  — see https://github.com/LibreSCRS/LibreMiddleware
EOF

# ---------------------------------------------------------------------------
# Package into compressed tarball (preserving symlinks)
# ---------------------------------------------------------------------------
OUTPUT="$PROJECT_ROOT/LibreSCRS-pkcs11-$VERSION-linux-$ARCH.tar.gz"
echo "Creating tarball..."
tar -czf "$OUTPUT" -C "$STAGING_PARENT" "$PKG_NAME"

echo ""
echo "Tarball created: $OUTPUT"
