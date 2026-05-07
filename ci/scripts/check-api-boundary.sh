#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
#
# check-api-boundary.sh — forbids internal LM headers in LC production code.
#
# Exits 0 if every `#include` in src/, plugins/, and test/ resolves to a
# public LibreSCRS header (or an LC-internal header). Any internal LM
# header include listed in FORBIDDEN_PATTERNS fails the check.
#
# Forbidden set rationale (post-Phase 7):
#   * <libresign/...>  — LibreSign has no public surface; every header
#     lives under LM's lib/libresign/src/.
#   * <smartcard/apdu.h>, <smartcard/ber.h>, <smartcard/tlv.h>,
#     <smartcard/ipcsc_scan_provider.h>, <smartcard/pcsc_scan_provider.h>,
#     <smartcard/pkcs11_card_provider.h>
#       — bucket-B headers relocated to lib/smartcard/src/ in Phase 6.
#         They are unreachable on the public include path today; keeping
#         them in the forbidden list is pure regression protection against
#         future re-exposure.
#
# Bucket-C smartcard headers (pcsc_connection.h, secure_buffer.h,
# monitor.h, monitor_event.h) are INTENTIONALLY NOT forbidden — they are
# the LC bridge surface Phase 7 preserved until LC migrates its Qt
# listener + async reader off them.
#
# TODO(phase-8-followup): once LC Qt listener and AsyncCardReader finish
# their migration off bucket-C, add the four remaining smartcard headers
# — <smartcard/pcsc_connection.h>, <smartcard/secure_buffer.h>,
# <smartcard/monitor.h>, <smartcard/monitor_event.h> — to FORBIDDEN_PATTERNS
# and expect those to disappear from LM's public include tree entirely.
#
# Match semantics: plain substring grep (no comment / #if 0 stripping).
# A commented-out `// #include <libresign/...>` WILL flag — this is
# intentional strictness; the reviewer must delete the comment rather
# than re-enabling it silently later.
#
# `.apiboundary-allow.txt` lists files granted exception status during
# migration windows. Should be EMPTY by end of 4.0.

set -euo pipefail

cd "$(dirname "$0")/../.."   # repo root

FORBIDDEN_PATTERNS=(
    '#include <libresign/'
    '#include <smartcard/apdu.h>'
    '#include <smartcard/ber.h>'
    '#include <smartcard/tlv.h>'
    '#include <smartcard/ipcsc_scan_provider.h>'
    '#include <smartcard/pcsc_scan_provider.h>'
    '#include <smartcard/pkcs11_card_provider.h>'
)
SEARCH_PATHS=(src plugins test)
EXCLUDE_FILE=.apiboundary-allow.txt

violations=0
for pat in "${FORBIDDEN_PATTERNS[@]}"; do
    while IFS= read -r match; do
        path=${match%%:*}
        if [[ -f $EXCLUDE_FILE ]] && grep -Fxq "$path" "$EXCLUDE_FILE"; then
            continue   # explicitly allowlisted
        fi
        echo "FORBIDDEN: $match"
        violations=$((violations + 1))
    done < <(grep -rnF "$pat" "${SEARCH_PATHS[@]}" --include='*.h' --include='*.cpp' || true)
done

if [[ $violations -gt 0 ]]; then
    echo
    echo "$violations API-boundary violations."
    echo "See .apiboundary-allow.txt for the explicit allowlist (should be empty post-4.0 merge)."
    exit 1
fi

echo "API boundary clean."
