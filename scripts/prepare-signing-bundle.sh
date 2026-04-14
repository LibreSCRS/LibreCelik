#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Prepare a signing/ bundle containing a jlink custom JRE, extracted Spring Boot
# JAR, and pre-built CDS archive for fast startup.
#
# Usage:  ./scripts/prepare-signing-bundle.sh <fat-jar-path> <output-dir> [java-home]
#
#   fat-jar-path  Path to the Spring Boot fat JAR (e.g. dss-service-1.0.0-SNAPSHOT.jar)
#   output-dir    Directory where signing/ will be created
#   java-home     Optional JDK home (auto-detected from java on PATH if omitted)
#
# Called by build-appimage.sh, build-dmg.sh, and CI.

set -euo pipefail

# ---------------------------------------------------------------------------
# Arguments
# ---------------------------------------------------------------------------
if [[ $# -lt 2 ]]; then
    echo "Usage: $0 <fat-jar-path> <output-dir> [java-home]"
    exit 1
fi

FAT_JAR="$(cd "$(dirname "$1")" && pwd)/$(basename "$1")"
OUTPUT_DIR="$2"
SIGNING_DIR="$OUTPUT_DIR/signing"

# ---------------------------------------------------------------------------
# Resolve JAVA_HOME
# ---------------------------------------------------------------------------
if [[ -n "${3:-}" ]]; then
    JAVA_HOME="$3"
else
    # Auto-detect from java on PATH, resolving symlinks to find JDK root
    JAVA_BIN="$(command -v java 2>/dev/null || true)"
    if [[ -z "$JAVA_BIN" ]]; then
        echo "ERROR: java not found on PATH and no java-home argument provided."
        exit 1
    fi
    # Resolve symlinks (readlink -f on Linux, realpath on macOS)
    if command -v readlink &>/dev/null && readlink -f "$JAVA_BIN" &>/dev/null; then
        JAVA_BIN="$(readlink -f "$JAVA_BIN")"
    elif command -v realpath &>/dev/null; then
        JAVA_BIN="$(realpath "$JAVA_BIN")"
    fi
    # java binary is at $JAVA_HOME/bin/java
    JAVA_HOME="$(cd "$(dirname "$JAVA_BIN")/.." && pwd)"
fi

if [[ ! -x "$JAVA_HOME/bin/jlink" ]]; then
    echo "ERROR: jlink not found at $JAVA_HOME/bin/jlink"
    echo "       Provide a full JDK (not JRE) as the java-home argument."
    exit 1
fi

if [[ ! -f "$FAT_JAR" ]]; then
    echo "ERROR: Fat JAR not found: $FAT_JAR"
    exit 1
fi

echo "Fat JAR:    $FAT_JAR"
echo "Output dir: $OUTPUT_DIR"
echo "JAVA_HOME:  $JAVA_HOME"
echo "Java:       $("$JAVA_HOME/bin/java" -version 2>&1 | head -1)"
echo ""

# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------
# Cross-platform file mtime in epoch seconds
file_mtime() {
    if stat -c %Y "$1" &>/dev/null 2>&1; then
        stat -c %Y "$1"     # Linux
    else
        stat -f %m "$1"     # macOS
    fi
}

# ---------------------------------------------------------------------------
# Step 0 — Clean output
# ---------------------------------------------------------------------------
echo "==> Cleaning $SIGNING_DIR"
rm -rf "$SIGNING_DIR"
mkdir -p "$SIGNING_DIR"

# ---------------------------------------------------------------------------
# Step 1 — jlink custom JRE
# ---------------------------------------------------------------------------
JLINK_MODULES="java.base,java.compiler,java.desktop,java.instrument,java.management,java.naming,java.net.http,java.prefs,java.scripting,java.security.jgss,java.sql,java.xml.crypto,jdk.jfr,jdk.unsupported"

echo "==> Creating jlink custom JRE"
echo "    Modules: $JLINK_MODULES"

"$JAVA_HOME/bin/jlink" \
    --add-modules "$JLINK_MODULES" \
    --strip-debug \
    --no-man-pages \
    --no-header-files \
    --compress zip-6 \
    --output "$SIGNING_DIR/jre"

if [[ ! -x "$SIGNING_DIR/jre/bin/java" ]]; then
    echo "ERROR: jlink failed — $SIGNING_DIR/jre/bin/java not found."
    exit 1
fi

JRE_SIZE=$(du -sh "$SIGNING_DIR/jre" | cut -f1)
echo "    JRE size: $JRE_SIZE"

# ---------------------------------------------------------------------------
# Step 2 — Generate base CDS archive
# ---------------------------------------------------------------------------
echo "==> Generating base CDS archive"

"$SIGNING_DIR/jre/bin/java" -Xshare:dump 2>&1 | tail -1

BASE_CDS="$SIGNING_DIR/jre/lib/server/classes.jsa"
if [[ ! -f "$BASE_CDS" ]]; then
    echo "ERROR: Base CDS archive not found at $BASE_CDS"
    exit 1
fi
echo "    Base CDS: $(du -h "$BASE_CDS" | cut -f1)"

# ---------------------------------------------------------------------------
# Step 3 — Extract Spring Boot fat JAR
# ---------------------------------------------------------------------------
echo "==> Extracting Spring Boot fat JAR"

# Spring Boot extract requires an empty destination, so extract to a temp dir
# then move contents into signing/.
EXTRACT_TMP="$(mktemp -d)"
"$SIGNING_DIR/jre/bin/java" \
    -Djarmode=tools \
    -jar "$FAT_JAR" \
    extract --destination "$EXTRACT_TMP/app"

# Move extracted contents (thin JAR + lib/) into signing/
mv "$EXTRACT_TMP/app"/*.jar "$SIGNING_DIR/"
mv "$EXTRACT_TMP/app/lib" "$SIGNING_DIR/"
rm -rf "$EXTRACT_TMP"

# The extract command preserves the original filename (e.g. dss-service-1.0.0-SNAPSHOT.jar)
THIN_JAR="$(find "$SIGNING_DIR" -maxdepth 1 -name '*.jar' -type f | head -1)"
if [[ -z "$THIN_JAR" ]]; then
    echo "ERROR: No thin JAR found in $SIGNING_DIR after extraction."
    exit 1
fi
echo "    Thin JAR: $(basename "$THIN_JAR")"

if [[ ! -d "$SIGNING_DIR/lib" ]]; then
    echo "ERROR: lib/ directory not found — extraction may have failed."
    exit 1
fi

LIB_COUNT=$(find "$SIGNING_DIR/lib" -name '*.jar' | wc -l)
echo "    Dependencies: $LIB_COUNT JARs in lib/"

# ---------------------------------------------------------------------------
# Step 4 — CDS training run (application archive)
# ---------------------------------------------------------------------------
echo "==> Running CDS training"

mkdir -p "$SIGNING_DIR/cds"

# Run the app just long enough to load classes, then exit on context refresh.
# Suppress CDS proxy-class warnings (harmless) but preserve real errors.
"$SIGNING_DIR/jre/bin/java" \
    -XX:ArchiveClassesAtExit="$SIGNING_DIR/cds/application.jsa" \
    -Dspring.context.exit=onRefresh \
    --enable-native-access=ALL-UNNAMED \
    -jar "$THIN_JAR" 2>&1 \
    | grep -v -e "Skipping.*proxy" \
              -e "Skipping.*Unsupported location" \
              -e "Failed verification" \
              -e "CDS archive was created" \
              -e "\[cds\]" \
    || true

APP_CDS="$SIGNING_DIR/cds/application.jsa"
if [[ ! -f "$APP_CDS" ]]; then
    echo "ERROR: Application CDS archive not found at $APP_CDS"
    exit 1
fi
echo "    App CDS: $(du -h "$APP_CDS" | cut -f1)"

# ---------------------------------------------------------------------------
# Step 5 — Write mtime stamp
# ---------------------------------------------------------------------------
file_mtime "$FAT_JAR" > "$SIGNING_DIR/cds/.jar-mtime"
echo "    Mtime stamp: $(cat "$SIGNING_DIR/cds/.jar-mtime")"

# ---------------------------------------------------------------------------
# Summary
# ---------------------------------------------------------------------------
echo ""
echo "=== Signing bundle ready ==="
echo "  JRE:          $SIGNING_DIR/jre/ ($JRE_SIZE)"
echo "  Base CDS:     $(du -h "$BASE_CDS" | cut -f1)"
echo "  Thin JAR:     $(basename "$THIN_JAR")"
echo "  Dependencies: $LIB_COUNT JARs"
echo "  App CDS:      $(du -h "$APP_CDS" | cut -f1)"
echo "  Total:        $(du -sh "$SIGNING_DIR" | cut -f1)"
