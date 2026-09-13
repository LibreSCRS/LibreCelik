#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""Refuse to publish a release whose bill of materials is missing or empty.

The producer already refuses to WRITE an empty bill -- it is not named here
on purpose, because the wiring gate reads prose as well as `run:` lines and a
filename in a docstring keeps a script looking called after its last real
caller is gone. This refuses to PUBLISH a set that has no bill at all -- a
build job that did not upload one, or a download step that did not merge it.
Both of those are silent otherwise, and the failure they hide is the one
this whole path exists to end: a signed document carrying "components": []
beside a bundle that ships OpenSSL, Qt and curl.

What it reads is the document, not the job that produced it: the bill must
parse, name CycloneDX, carry a non-empty components array, and every
component must have a name. It does not verify that the bill describes the
artefact beside it -- that is the producer's job, and the producer derives
both from one walk of one tree.

Usage:  check-sbom.py <bill.cdx.json> [<bill.cdx.json> ...]
"""

import json
import sys


def check(path: str) -> str:
    """Return an error string, or "" when the bill is publishable."""
    try:
        with open(path, "r", encoding="utf-8") as fh:
            doc = json.load(fh)
    except FileNotFoundError:
        return f"{path}: missing — the build job published no bill of materials"
    except (OSError, ValueError) as exc:
        return f"{path}: unreadable ({exc})"

    if doc.get("bomFormat") != "CycloneDX":
        return f"{path}: not a CycloneDX document"
    components = doc.get("components")
    if not isinstance(components, list):
        return f"{path}: no components array"
    if not components:
        return (
            f"{path}: zero components — a signed bill claiming this artefact "
            f"bundles nothing must not be published"
        )
    nameless = [c for c in components if not c.get("name")]
    if nameless:
        return f"{path}: {len(nameless)} components carry no name"
    print(f"check-sbom: {path}: {len(components)} components")
    return ""


def main(argv):
    if not argv:
        print("usage: check-sbom.py <bill.cdx.json> ...", file=sys.stderr)
        return 2
    errors = [e for e in (check(p) for p in argv) if e]
    for e in errors:
        print(f"::error::{e}", file=sys.stderr)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
