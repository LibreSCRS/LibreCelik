#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
#
# pre-commit-i18n.sh — opt-in local hook for fast incremental i18n audit.
#
# Runs tools/i18n_audit.py in --strict --diff-since=HEAD mode so only
# the files staged for commit are scanned. The full audit (all of src,
# plugins, test/) runs on PR CI; this hook is a quick local pre-flight
# that catches regressions before they reach the remote.
#
# Activate by setting `git config core.hooksPath` to a directory
# containing a `pre-commit` symlink to this script, OR by chaining
# from an existing pre-commit hook. CONTRIBUTING.md documents both
# options.
#
# Skip with `git commit --no-verify`. PR CI's i18n-audit job (see
# .github/workflows/ci.yml) is the authoritative gate.

set -eu

PYTHON="${PYTHON:-python3}"
if ! command -v "$PYTHON" >/dev/null 2>&1; then
    echo "pre-commit-i18n: python3 not found on PATH." >&2
    echo "  Install via your distro package manager, or set PYTHON=/path/to/python3." >&2
    exit 2
fi

# Locate repo root from this script (works whether invoked via
# .git/hooks symlink or core.hooksPath).
REPO_ROOT="$(git rev-parse --show-toplevel)"
AUDIT="$REPO_ROOT/tools/i18n_audit.py"

if [[ ! -f "$AUDIT" ]]; then
    echo "pre-commit-i18n: audit tool not found at $AUDIT — skipping." >&2
    exit 0
fi

# Diff-mode: only scan files modified since HEAD that are now staged.
# The audit's --diff-since flag understands a git revision and walks
# the tree on its own.
if ! "$PYTHON" "$AUDIT" --strict --diff-since=HEAD; then
    echo >&2
    echo "pre-commit-i18n: i18n audit reported active findings in staged files." >&2
    echo "  Fix or allowlist (// i18n-audit: ignore D<n>, <reason>) before committing." >&2
    echo "  Full audit doc: tools/README-i18n-audit.md" >&2
    echo "  Skip (CI will re-run):  git commit --no-verify" >&2
    exit 1
fi
