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
# --abbrev=0 is deliberate HERE and nowhere else: the artefact file name wants
# the release number, not a describe string with a commit hash in it. The
# resolution itself is shared with the other packaging script and mirrors
# cmake/GitVersion.cmake — it refuses a foreign repository's tags, accepts only
# version-shaped ones, and falls back to the VERSION file.
# shellcheck source=../project-version.sh
. "$SCRIPT_DIR/../project-version.sh"
VERSION="$(project_version "$PROJECT_ROOT")"
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
find "$FILTERED_PLUGINS/imageformats" -name "kimg_*.so" -delete

# Remove imageformat plugins whose shared-library dependencies are missing on
# this system. This avoids linuxdeploy-plugin-qt failing when e.g. libqtiff.so
# links a libtiff version not present on the build host.
echo "Checking imageformat plugin dependencies..."
for plugin in "$FILTERED_PLUGINS/imageformats"/*.so; do
    [[ -f "$plugin" ]] || continue
    if ! ldd "$plugin" 2>/dev/null | grep -q "not found"; then
        continue
    fi
    missing=$(ldd "$plugin" 2>/dev/null | grep "not found" | awk '{print $1}' | tr '\n' ' ')
    echo "  removing $(basename "$plugin") (missing: $missing)"
    rm "$plugin"
done

echo "Filtered imageformats:"
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
# Phase 2b — Copy the LibreSCRS GUI plugins into AppDir.
#
# The app resolves these at runtime via ../lib/ relative to the binary.
# Card access is not bundled at all: it lives in the separate agent service,
# which this AppImage never ships (the app guides the user to install it),
# so there is no card-side plugin directory, PKCS#11 module or certificate
# store to stage here.
# ---------------------------------------------------------------------------
GUI_PLUGIN_DIR="$BUILD_DIR/gui-plugins"

# --- Agent client library ---
# The executable links it (DT_NEEDED), so linuxdeploy stages it in Phase 1
# together with the Qt libraries and rewrites its RPATH. The wire codec is a
# static archive folded into this library, so there is no second shared object
# to look for. Assert it landed: without it the AppImage cannot reach the
# agent at all, and a silent miss would only surface on the user's machine.
if ! ls "$APPDIR/usr/lib"/liblibrescrs-agentclient-qt.so* &>/dev/null; then
    echo "ERROR: agent client library not found in $APPDIR/usr/lib —"
    echo "       linuxdeploy did not stage liblibrescrs-agentclient-qt.so*."
    echo "       Check that $BINARY links it and that its build-tree RPATH resolves."
    exit 1
fi
echo "Agent client library in AppDir:"
ls "$APPDIR/usr/lib"/liblibrescrs-agentclient-qt.so*

# --- DSS signing bundle (JRE + JAR) ---
# Skipped: native signing backend is the default and does not require Java.
# DSS is deprecated. To re-enable, set SIGNING_BACKEND=dss and uncomment.

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
# On macOS, Qt uses ImageIO (built-in JP2 support). On Linux, the libqjp2.so
# plugin from Qt's qtimageformats module is needed. It links libopenjp2 (Qt's
# official build) or libjasper (KDE's kimageformats).
#
# aqtinstall Qt does not ship libqjp2.so, so on CI we install the system
# qt6-image-formats-plugins package and search both the aqtinstall path and
# the system Qt plugin path (/usr/lib/*/qt6/plugins).
echo "Bundling JPEG2000 support (eMRTD images)..."
JP2_PLUGIN=""
for search_dir in "$QT_PLUGINS_SYSTEM/imageformats" /usr/lib/*/qt6/plugins/imageformats; do
    if [[ -f "$search_dir/libqjp2.so" ]]; then
        JP2_PLUGIN="$search_dir/libqjp2.so"
        break
    fi
done
if [[ -n "$JP2_PLUGIN" ]]; then
    if [[ ! -f "$APPDIR/usr/plugins/imageformats/libqjp2.so" ]]; then
        cp "$JP2_PLUGIN" "$APPDIR/usr/plugins/imageformats/"
        patchelf --set-rpath '$ORIGIN/../../lib:$ORIGIN' "$APPDIR/usr/plugins/imageformats/libqjp2.so"
        echo "  copied libqjp2.so (from $JP2_PLUGIN)"
    else
        echo "  libqjp2.so already in AppDir"
    fi
    # Bundle JP2 codec libraries that the plugin needs at runtime
    for pattern in 'libjasper\.so\.\d+' 'libopenjp2\.so\.\d+'; do
        LIB_PATH=$(ldconfig -p 2>/dev/null | grep -oP "/\\S*${pattern}" | head -1 || true)
        if [[ -n "$LIB_PATH" && -f "$LIB_PATH" ]]; then
            LIB_NAME=$(basename "$LIB_PATH")
            if [[ ! -f "$APPDIR/usr/lib/$LIB_NAME" ]]; then
                cp "$LIB_PATH" "$APPDIR/usr/lib/"
                patchelf --set-rpath '$ORIGIN' "$APPDIR/usr/lib/$LIB_NAME"
                echo "  copied $LIB_NAME"
            else
                echo "  $LIB_NAME already in AppDir"
            fi
        fi
    done
else
    echo "  WARNING: libqjp2.so not found — JPEG2000 images (eMRTD) will not display"
    echo "  Install qt6-image-formats-plugins (system) or qtimageformats (aqtinstall)"
fi

# ---------------------------------------------------------------------------
# Phase 2c — Restore leancrypto from the host, unpatched.
#
# patchelf — the rpath rewrite linuxdeploy runs over everything it bundles —
# corrupts this ELF's symbol lookup. The patched copy fails dlopen with an
# undefined symbol its own .dynsym defines; loaded as a dependency inside a
# larger graph it instead takes ld.so down walking the GNU-hash chains.
# Printing is where it surfaced: QPrinter makes QFactoryLoader dlopen the CUPS
# print-support plugin, which reaches leancrypto through libcups and libgnutls,
# and the app died there.
#
# The library needs no RUNPATH at all — its only NEEDED is libc, and the
# dependents that load it find it through their own $ORIGIN — so the repair is
# a plain copy of the host's file with NO patchelf after it. It runs here,
# after every other patchelf step, for exactly that reason.
# ---------------------------------------------------------------------------
echo "Restoring unpatched leancrypto..."
# Read the loader cache ONCE into a variable: awk exiting on its first match
# closes the pipe under `ldconfig`, and `set -o pipefail` turns that SIGPIPE
# into a build failure.
HOST_SONAMES="$(ldconfig -p 2>/dev/null || true)"
for bundled in "$APPDIR/usr/lib"/libleancrypto*.so*; do
    [[ -f "$bundled" ]] || continue
    soname="$(basename "$bundled")"
    pristine="$(awk -v n="$soname" '$1 == n { print $NF; exit }' <<< "$HOST_SONAMES")"
    if [[ -z "$pristine" || ! -f "$pristine" ]]; then
        echo "ERROR: $soname is bundled but no host copy resolves through ldconfig -p."
        echo "       The bundled copy is patchelf-corrupted and cannot be shipped."
        exit 1
    fi
    cp "$pristine" "$bundled"
    echo "  restored $soname from $pristine (no patchelf)"
done

# ---------------------------------------------------------------------------
# Verify every bundled shared library has a documented license. Runs after the
# AppDir is fully assembled (all libs + plugins deployed) and before sealing.
# Fail-closed: an unmapped/missing/hash-mismatched library aborts the build.
# ---------------------------------------------------------------------------
echo "Verifying bundled-license completeness..."
python3 "$PROJECT_ROOT/ci/scripts/check-bundled-licenses.py" \
    --check "$APPDIR" \
    --manifest "$PROJECT_ROOT/licenses/manifest.json" \
    --platform linux || {
        echo "ERROR: bundled-license check failed — a bundled library lacks a documented license (see ::error:: lines above)." >&2
        exit 1
    }

# ---------------------------------------------------------------------------
# Phase 2e — dlopen audit. THE gate that keeps a broken object out of a
# shipped artifact.
#
# `ldd` is not enough: the leancrypto corruption above left a library whose
# dependencies all resolved and whose .dynsym still defined every symbol, and
# it failed only when something asked the loader to bind them. So every object
# the AppImage ships is opened for real, with RTLD_NOW so undefined symbols
# are found here rather than on a user's machine in whatever feature happens
# to reach them first.
#
# One process per object: a corrupted object can crash the loader outright,
# and a driver that dies must still name what it died on.
# ---------------------------------------------------------------------------
need cc

DLOPEN_AUDIT="$TOOLS_DIR/dlopen-audit"
cat > "$TOOLS_DIR/dlopen-audit.c" << 'AUDIT_EOF'
/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Open one shared object with RTLD_NOW; print dlerror() and fail if it
   cannot be loaded. Deliberately tiny: it must not itself pull in anything
   that could mask a loader failure. */
#include <dlfcn.h>
#include <stdio.h>

int main(int argc, char **argv)
{
    void *handle;

    if (argc != 2) {
        fprintf(stderr, "usage: dlopen-audit <path-to-so>\n");
        return 2;
    }
    handle = dlopen(argv[1], RTLD_NOW);
    if (handle == NULL) {
        fprintf(stderr, "%s\n", dlerror());
        return 1;
    }
    return 0;
}
AUDIT_EOF
cc -o "$DLOPEN_AUDIT" "$TOOLS_DIR/dlopen-audit.c"

echo ""
echo "Auditing bundled objects with dlopen(RTLD_NOW)..."
AUDIT_DIRS=("$APPDIR/usr/lib")
[[ -d "$APPDIR/usr/plugins" ]] && AUDIT_DIRS+=("$APPDIR/usr/plugins")
AUDIT_TOTAL=0
AUDIT_FAILED=0
while IFS= read -r object; do
    AUDIT_TOTAL=$((AUDIT_TOTAL + 1))
    set +e
    AUDIT_ERR=$(LD_LIBRARY_PATH="$APPDIR/usr/lib" "$DLOPEN_AUDIT" "$object" 2>&1)
    AUDIT_RC=$?
    set -e
    [[ $AUDIT_RC -eq 0 ]] && continue
    AUDIT_FAILED=$((AUDIT_FAILED + 1))
    echo "  FAILED ${object#"$APPDIR"/}"
    if [[ -n "$AUDIT_ERR" ]]; then
        echo "         $AUDIT_ERR"
    else
        echo "         dlopen driver died (exit $AUDIT_RC) — the loader itself went down"
    fi
done < <(find "${AUDIT_DIRS[@]}" -type f -name "*.so*" | sort)

if [[ $AUDIT_FAILED -ne 0 ]]; then
    echo "ERROR: $AUDIT_FAILED of $AUDIT_TOTAL bundled objects cannot be dlopen'd — not packaging." >&2
    exit 1
fi
echo "  $AUDIT_TOTAL bundled objects loaded cleanly."

# ---------------------------------------------------------------------------
# Phase 3 — Package with appimagetool.
# ---------------------------------------------------------------------------
echo ""
echo "Packaging AppImage..."
ARCH=x86_64 "$APPIMAGETOOL" "$APPDIR" "$OUTPUT"

echo ""
echo "AppImage created: $OUTPUT"
