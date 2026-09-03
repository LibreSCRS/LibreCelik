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
# Forbidden set rationale:
#   * <libresign/...>  — LibreSign has no public surface; every header
#     lives under LM's lib/libresign/src/.
#   * <smartcard/apdu.h>, <smartcard/ber.h>, <smartcard/tlv.h>,
#     <smartcard/ipcsc_scan_provider.h>, <smartcard/pcsc_scan_provider.h>,
#     <smartcard/pkcs11_card_provider.h>
#       — internal headers that live under lib/smartcard/src/. They are
#         unreachable on the public include path today; keeping them in
#         the forbidden list is pure regression protection against future
#         re-exposure.
#
#   * <smartcard/pcsc_connection.h>, <smartcard/secure_buffer.h>,
#     <smartcard/monitor.h>, <smartcard/monitor_event.h>
#       — the former LC bridge surface. They were left reachable while LC
#         still drove a Qt listener and an async reader off them; neither
#         exists here any more and no file under src/, plugins/ or test/
#         includes them, so they join the forbidden set.
#
# Match semantics: plain substring grep (no comment / #if 0 stripping).
# A commented-out `// #include <libresign/...>` WILL flag — this is
# intentional strictness; the reviewer must delete the comment rather
# than re-enabling it silently later.
#
# There is no allowlist. The file that held one said it should be empty by
# the end of 4.0; it was, two majors later, so the exception mechanism went
# with it rather than waiting to be used by accident.

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
    '#include <smartcard/pcsc_connection.h>'
    '#include <smartcard/secure_buffer.h>'
    '#include <smartcard/monitor.h>'
    '#include <smartcard/monitor_event.h>'
)
SEARCH_PATHS=(src plugins test)

violations=0
for pat in "${FORBIDDEN_PATTERNS[@]}"; do
    while IFS= read -r match; do
        echo "FORBIDDEN: $match"
        violations=$((violations + 1))
    done < <(grep -rnF "$pat" "${SEARCH_PATHS[@]}" --include='*.h' --include='*.cpp' || true)
done

if [[ $violations -gt 0 ]]; then
    echo
    echo "$violations API-boundary violations."
    exit 1
fi

echo "API boundary clean."
