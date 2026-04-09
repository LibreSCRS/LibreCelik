#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Build a LibreCelik AppImage.
#
# Usage:  ./scripts/linux/build-appimage.sh [BUILD_DIR]
#   BUILD_DIR  CMake build directory (default: build)
#
# Prerequisites:
#   - CMake Release build already compiled in BUILD_DIR
#   - Qt6 development tools on PATH (qmake6)
#   - pcscd (libpcsclite) installed on the build machine so the binary links
#   - wget or curl
#
# The script auto-downloads linuxdeploy + linuxdeploy-plugin-qt + appimagetool
# into scripts/linux/tools/ if they are not already present.
#
# Output: LibreCelik-<VERSION>-x86_64.AppImage in the project root.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"
BUILD_DIR="${1:-$PROJECT_ROOT/build}"
TOOLS_DIR="$SCRIPT_DIR/tools"
APPDIR="$PROJECT_ROOT/AppDir"

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
need() { command -v "$1" &>/dev/null || { echo "ERROR: '$1' not found. Please install it."; exit 1; }; }
download() {
    local url="$1" dest="$2"
    if command -v wget &>/dev/null; then wget -q --show-progress -O "$dest" "$url"
    elif command -v curl &>/dev/null; then curl -L --progress-bar -o "$dest" "$url"
    else echo "ERROR: need wget or curl"; exit 1; fi
}

need cmake

# ---------------------------------------------------------------------------
# Resolve Qt paths
# ---------------------------------------------------------------------------
QMAKE=$(command -v qmake6 || command -v qmake || true)
if [[ -z "$QMAKE" ]]; then
    echo "ERROR: qmake not found. Ensure Qt6 is on PATH."
    exit 1
fi
QT_PLUGINS_SYSTEM="$("$QMAKE" -query QT_INSTALL_PLUGINS)"
echo "Qt qmake:   $QMAKE"
echo "Qt plugins: $QT_PLUGINS_SYSTEM"

# ---------------------------------------------------------------------------
# Auto-download linuxdeploy tools
# ---------------------------------------------------------------------------
mkdir -p "$TOOLS_DIR"

LINUXDEPLOY="$TOOLS_DIR/linuxdeploy-x86_64.AppImage"
LINUXDEPLOY_QT="$TOOLS_DIR/linuxdeploy-plugin-qt-x86_64.AppImage"
APPIMAGETOOL="$TOOLS_DIR/appimagetool-x86_64.AppImage"

if [[ ! -x "$LINUXDEPLOY" ]]; then
    echo "Downloading linuxdeploy..."
    download "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage" "$LINUXDEPLOY"
    chmod +x "$LINUXDEPLOY"
fi

if [[ ! -x "$LINUXDEPLOY_QT" ]]; then
    echo "Downloading linuxdeploy-plugin-qt..."
    download "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage" "$LINUXDEPLOY_QT"
    chmod +x "$LINUXDEPLOY_QT"
fi

if [[ ! -x "$APPIMAGETOOL" ]]; then
    echo "Downloading appimagetool..."
    download "https://github.com/AppImage/appimagetool/releases/download/continuous/appimagetool-x86_64.AppImage" "$APPIMAGETOOL"
    chmod +x "$APPIMAGETOOL"
fi

# ---------------------------------------------------------------------------
# Determine version from git tag
# ---------------------------------------------------------------------------
VERSION=$(git -C "$PROJECT_ROOT" describe --tags --abbrev=0 2>/dev/null || echo "dev")
echo "Building AppImage for version: $VERSION"

# ---------------------------------------------------------------------------
# Locate the built binary
# ---------------------------------------------------------------------------
BINARY="$BUILD_DIR/src/LibreCelik"
if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: Binary not found at $BINARY"
    echo "       Run: cmake --build $BUILD_DIR first"
    exit 1
fi

# ---------------------------------------------------------------------------
# Build a filtered Qt plugin directory.
#
# On KDE desktops (Manjaro, Fedora KDE, etc.) /usr/lib/qt6/plugins/imageformats/
# contains many KDE-extras (kimg_*.so) that have dependencies not installed on
# all systems (e.g. libjxrglue for kimg_jxr.so).  LibreCelik embeds all its
# images in the Qt resource system, so it does not need these extras.
#
# Strategy: copy the ENTIRE Qt plugins tree (so all subdirectory categories
# exist and linuxdeploy-plugin-qt doesn't crash on missing dirs), then remove
# only the KDE-specific kimg_*.so imageformat extras.
# We point linuxdeploy-plugin-qt at this tree via the qmake wrapper.
# ---------------------------------------------------------------------------
FILTERED_PLUGINS="$(mktemp -d)/qt_plugins"
echo "Building filtered Qt plugin dir: $FILTERED_PLUGINS"

# Copy only *.so files from the Qt plugins directory (skip non-.so KDE extras).
# We intentionally skip sub-directories whose contents are non-Qt plugins
# (e.g. akonadi_*, digikam, dolphin, kwin, …) by filtering to known Qt plugin
# categories needed at runtime.
QT_PLUGIN_CATEGORIES=(
    imageformats
    platforms
    platforminputcontexts
    xcbglintegrations
    egldeviceintegrations
    printsupport
    styles
    iconengines
    tls
    networkinformation
    generic
)
for cat in "${QT_PLUGIN_CATEGORIES[@]}"; do
    src="$QT_PLUGINS_SYSTEM/$cat"
    [[ -d "$src" ]] || continue
    mkdir -p "$FILTERED_PLUGINS/$cat"
    for f in "$src"/*.so; do
        [[ -f "$f" ]] && cp "$f" "$FILTERED_PLUGINS/$cat/"
    done
done

# Remove KDE-specific imageformat extras — they have deps (e.g. libjxrglue,
# libjasper) not installed on non-KDE systems.
# LibreCelik uses the Qt resource system for all its images; it needs only the
# standard Qt imageformat plugins (libq*.so).
find "$FILTERED_PLUGINS/imageformats" -name "kimg_*.so" -delete
echo "Filtered imageformats (libq* only):"
ls "$FILTERED_PLUGINS/imageformats/"

# ---------------------------------------------------------------------------
# Create a qmake wrapper that redirects all Qt plugin-path results to our
# filtered directory. linuxdeploy-plugin-qt uses two call styles:
#
#   qmake -query QT_INSTALL_PLUGINS   → single value, we return filtered dir
#   qmake -query                      → all variables, we pass through but
#                                        replace every *PLUGINS* line
#
# Both styles are intercepted so the plugin never sees the KDE system path.
# ---------------------------------------------------------------------------
QMAKE_WRAPPER="$TOOLS_DIR/qmake_wrapper"
cat > "$QMAKE_WRAPPER" << WRAPPER_EOF
#!/usr/bin/env bash
if [[ "\$*" == *"PLUGINS"* ]]; then
    # Single-variable query for a plugin path (e.g. -query QT_INSTALL_PLUGINS)
    echo "$FILTERED_PLUGINS"
elif [[ "\$*" == *"-query"* ]]; then
    # All-variables query — rewrite every *PLUGINS* line in the output.
    # qmake -query uses "KEY:value" format (colon, not equals sign).
    "$QMAKE" "\$@" | sed "s|^\\(QT[A-Z_]*PLUGINS\\):.*|\\1:$FILTERED_PLUGINS|g"
else
    exec "$QMAKE" "\$@"
fi
WRAPPER_EOF
chmod +x "$QMAKE_WRAPPER"

# ---------------------------------------------------------------------------
# Clean and prepare AppDir
# ---------------------------------------------------------------------------
rm -rf "$APPDIR"
mkdir -p "$APPDIR"

# ---------------------------------------------------------------------------
# Phase 1 — Deploy binary + Qt libs/plugins into AppDir.
#
# Excluded libraries (must come from the host system at runtime):
#   libpcsclite   — IPC client for pcscd; must match the running daemon
#   libGL/libEGL  — GPU drivers; device-specific
#   libdbus-1     — system IPC
#   libwayland-*  — compositor-specific
#
# We do NOT pass --output appimage here. linuxdeploy-plugin-qt may exit
# non-zero on KDE systems because kimg_*.so extras from /usr/lib/qt6/plugins/
# have dependencies unavailable on non-KDE systems (e.g. libjxrglue for
# kimg_jxr.so). The plugins are fully copied to AppDir before that check, so
# we tolerate the non-zero exit, strip kimg_* in Phase 2, then package cleanly.
# ---------------------------------------------------------------------------
export QMAKE="$QMAKE_WRAPPER"
export QT_PLUGIN_PATH="$FILTERED_PLUGINS"
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
export OUTPUT="$PROJECT_ROOT/LibreCelik-$VERSION-x86_64.AppImage"
export VERSION
# Manjaro (and other rolling distros) use .relr.dyn ELF sections that the
# bundled strip inside linuxdeploy's AppImage cannot parse. The binary is
# already Release-optimised so skipping strip is fine.
export NO_STRIP=1

set +e
"$LINUXDEPLOY" \
    --appdir "$APPDIR" \
    --executable "$BINARY" \
    --desktop-file "$PROJECT_ROOT/resources/linux/librecelik.desktop" \
    --icon-file "$PROJECT_ROOT/resources/smartcard-id-512.png" \
    --icon-filename librecelik \
    --exclude-library "libpcsclite.so*" \
    --exclude-library "libGL.so*" \
    --exclude-library "libEGL.so*" \
    --exclude-library "libGLdispatch.so*" \
    --exclude-library "libGLX.so*" \
    --exclude-library "libdbus-1.so*" \
    --exclude-library "libwayland-client.so*" \
    --exclude-library "libwayland-server.so*" \
    --plugin qt
DEPLOY_EXIT=$?
set -e

# ---------------------------------------------------------------------------
# Phase 2 — Remove KDE-specific imageformat plugins.
#
# LibreCelik embeds all its images in the Qt resource system and does not load
# image files from disk, so no extra imageformat plugins are needed at runtime.
# kimg_*.so plugins come from KDE Frameworks and have deps (e.g. libjxrglue,
# libjasper) that are not installed on non-KDE systems.
# ---------------------------------------------------------------------------
KDE_PLUGINS=$(find "$APPDIR" -name "kimg_*.so" 2>/dev/null || true)
if [[ -n "$KDE_PLUGINS" ]]; then
    echo "Removing KDE-specific imageformat plugins from AppDir:"
    echo "$KDE_PLUGINS" | while read -r p; do echo "  $p"; done
    find "$APPDIR" -name "kimg_*.so" -delete
fi

# Verify the binary made it into AppDir regardless of exit code
if [[ ! -f "$APPDIR/usr/bin/LibreCelik" ]]; then
    echo "ERROR: linuxdeploy failed (exit $DEPLOY_EXIT) and AppDir is incomplete."
    echo "       Check the output above for the actual cause."
    exit 1
fi

if [[ $DEPLOY_EXIT -ne 0 ]] && [[ -z "$KDE_PLUGINS" ]]; then
    echo "WARNING: linuxdeploy-plugin-qt exited $DEPLOY_EXIT for an unknown reason."
    echo "         AppDir binary is present — attempting to package anyway."
fi

# ---------------------------------------------------------------------------
# Phase 2b — Copy LibreSCRS plugins (middleware + GUI) into AppDir.
#
# The app resolves these at runtime via ../lib/ relative to the binary.
# ---------------------------------------------------------------------------
MW_PLUGIN_DIR="$BUILD_DIR/plugins"
GUI_PLUGIN_DIR="$BUILD_DIR/gui-plugins"

echo "Copying middleware plugins..."
mkdir -p "$APPDIR/usr/lib/middleware-plugins"
for f in "$MW_PLUGIN_DIR"/lib*-plugin.so; do
    [[ -f "$f" ]] && cp "$f" "$APPDIR/usr/lib/middleware-plugins/"
done
ls "$APPDIR/usr/lib/middleware-plugins/"

echo "Copying GUI plugins..."
mkdir -p "$APPDIR/usr/lib/gui-plugins"
for f in "$GUI_PLUGIN_DIR"/*-gui-plugin.so; do
    [[ -f "$f" ]] && cp "$f" "$APPDIR/usr/lib/gui-plugins/"
done
ls "$APPDIR/usr/lib/gui-plugins/"

# Patch RPATH of GUI plugins so they find the bundled Qt libs at runtime.
# Without this, plugins retain the CI build-tree RPATH (e.g. /home/runner/...)
# and fall back to the system Qt, causing ABI mismatch failures.
echo "Patching GUI plugin RPATHs..."
for f in "$APPDIR/usr/lib/gui-plugins"/*.so; do
    patchelf --set-rpath '$ORIGIN/..' "$f"
done

# Bundle Qt libraries that GUI plugins need but the main binary doesn't link.
# linuxdeploy only processes the main executable's dependencies, so plugin-only
# deps like PrintSupport must be copied explicitly.
QT_LIB_DIR="$("$QMAKE" -query QT_INSTALL_LIBS)"
echo "Bundling extra Qt libs for GUI plugins..."
for lib in libQt6PrintSupport.so.6; do
    src="$QT_LIB_DIR/$lib"
    if [[ -f "$src" ]]; then
        cp "$src" "$APPDIR/usr/lib/"
        patchelf --set-rpath '$ORIGIN' "$APPDIR/usr/lib/$lib"
        echo "  copied $lib"
    else
        echo "  WARNING: $lib not found in $QT_LIB_DIR"
    fi
done

# Bundle JPEG2000 support for eMRTD travel documents.
# eMRTD cards store facial images and signatures in JPEG2000 (JP2) format.
# linuxdeploy-plugin-qt silently skips libqjp2.so when libjasper is missing
# from its resolution path, so we must copy both explicitly.
echo "Bundling JPEG2000 support (eMRTD images)..."
JP2_PLUGIN="$QT_PLUGINS_SYSTEM/imageformats/libqjp2.so"
JASPER_SO=$(ldconfig -p 2>/dev/null | grep -oP '/\S*libjasper\.so\.\d+' | head -1)
if [[ -f "$JP2_PLUGIN" && -n "$JASPER_SO" && -f "$JASPER_SO" ]]; then
    cp "$JP2_PLUGIN" "$APPDIR/usr/plugins/imageformats/"
    patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "$APPDIR/usr/plugins/imageformats/libqjp2.so"
    echo "  copied libqjp2.so"
    cp "$JASPER_SO" "$APPDIR/usr/lib/"
    patchelf --set-rpath '$ORIGIN' "$APPDIR/usr/lib/$(basename "$JASPER_SO")"
    echo "  copied $(basename "$JASPER_SO")"
else
    echo "  WARNING: libqjp2.so or libjasper not found — JPEG2000 images (eMRTD) will not display"
    [[ ! -f "$JP2_PLUGIN" ]] && echo "    missing: $JP2_PLUGIN"
    [[ -z "$JASPER_SO" || ! -f "$JASPER_SO" ]] && echo "    missing: libjasper.so"
fi

# ---------------------------------------------------------------------------
# Phase 3 — Package with appimagetool.
# ---------------------------------------------------------------------------
echo ""
echo "Packaging AppImage..."
ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo ""
echo "AppImage created: $OUTPUT"
