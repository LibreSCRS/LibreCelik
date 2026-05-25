#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""Fail-closed bundled-license checker for LibreCelik distributed artifacts.

This module provides the enumeration and matching primitives used to map
every bundled shared library in a packaging staging tree to a documented
license component. The matching is longest-specific-match-wins so that a
specific entry (e.g. a single GPL-licensed module) is never swept under a
broader glob covering a differently-licensed family.

Only the enumeration core lives here today. The fail-closed ``--check``
verdict logic and the ``--emit-candidates`` bootstrap mode are added on
top of these primitives.
"""

import argparse
import fnmatch
import hashlib
import json
import os
import re
import sys

# Match a ".so" or ".dylib" extension at a real boundary: the extension
# must be followed by a version separator (".") or the end of the name.
# This prevents a stray substring (e.g. "libfoo.solics") from being
# mis-normalized to "libfoo.so" and silently inheriting another
# component's license. Everything after the extension (version segments
# such as ".4.8.0") is stripped so "libcurl.so.4.8.0" maps to the same
# component as "libcurl.so".
_EXT = re.compile(r"\.(so|dylib)(\.|$)")


def normalize(basename: str) -> str:
    """Strip trailing version segments after the ``.so``/``.dylib`` extension.

    Examples::

        libcurl.so.4.8.0   -> libcurl.so
        libQt6Core.so.6.10.0 -> libQt6Core.so
        libicudata.so.74.2 -> libicudata.so
        libfoo.dylib       -> libfoo.dylib
        libfoo.solics      -> libfoo.solics (not a real .so boundary)
        plain-name         -> plain-name (no extension, returned unchanged)
    """
    m = _EXT.search(basename)
    if not m:
        return basename
    # m.end(1) is the end of the so/dylib group, excluding the trailing
    # separator captured by group 2.
    return basename[: m.end(1)]


def is_internal(name: str, internal_globs) -> bool:
    """Return True if ``name`` matches any glob in ``internal_globs``.

    Internal (LibreSCRS-owned) libraries carry the project's own license
    and are skipped by the bundle check.
    """
    return any(fnmatch.fnmatch(name, g) for g in internal_globs)


def match_component(name: str, components):
    """Return the most-specific component matching ``name``, else None.

    Globs are matched case-sensitively against the whole normalized
    basename. When several entries match, the one whose ``match`` pattern
    has the most non-wildcard characters (the most specific) wins, so a
    literal soname always beats a family glob.
    """
    best = None
    best_specificity = -1
    for c in components:
        if fnmatch.fnmatchcase(name, c["match"]):
            specificity = len(c["match"].replace("*", "").replace("?", ""))
            if specificity > best_specificity:
                best = c
                best_specificity = specificity
    return best


def enumerate_assets(root: str):
    """Yield ``(normalized_name, realpath)`` for every shared library.

    Walks the entire staging tree under ``root`` and yields each
    ``.so*``/``.dylib`` file exactly once, deduped by ``os.path.realpath``
    so a symlink and its target are counted a single time.
    """
    seen = set()
    for dirpath, _dirs, files in os.walk(root):
        for f in files:
            if not _EXT.search(f):
                continue
            full = os.path.join(dirpath, f)
            rp = os.path.realpath(full)
            if rp in seen:
                continue
            seen.add(rp)
            yield normalize(f), rp


def emit_candidates(root: str, manifest):
    """Enumerate unmapped bundled libraries under ``root``.

    Walks the staging tree via :func:`enumerate_assets` and returns one
    candidate manifest entry per normalized soname that is **not** yet
    accounted for. A soname is skipped when it:

    - matches an ``internal_sonames`` glob (LibreSCRS-owned), or
    - matches an ``exclude_not_bundled`` glob (deliberately not bundled
      by the packaging scripts), or
    - already resolves to an existing ``components`` entry.

    Each remaining (unmapped) name yields a placeholder entry with empty
    ``name``/``spdx``/``text``/``sha256`` for hand-authoring. The result
    is sorted by name and deduplicated.
    """
    internal = manifest.get("internal_sonames", [])
    excluded = manifest.get("exclude_not_bundled", [])
    components = manifest.get("components", [])

    unmapped = set()
    for name, _rp in enumerate_assets(root):
        if is_internal(name, internal):
            continue
        if any(fnmatch.fnmatch(name, g) for g in excluded):
            continue
        if match_component(name, components) is not None:
            continue
        unmapped.add(name)

    return [
        {"match": n, "name": "", "spdx": "", "text": "", "sha256": ""}
        for n in sorted(unmapped)
    ]


def _sha256_file(path: str) -> str:
    h = hashlib.sha256()
    with open(path, "rb") as fh:
        for chunk in iter(lambda: fh.read(65536), b""):
            h.update(chunk)
    return h.hexdigest()


def run_check(staging_root: str, manifest, manifest_dir: str) -> int:
    """Fail-closed verdict over a packaging staging tree.

    Enumerates every bundled shared library under ``staging_root`` and
    requires each one to either be internal/excluded, or map to a manifest
    component whose recorded license text exists and hashes verbatim. Any
    unmapped library, missing license text, or sha256 mismatch is an
    ``::error::`` and makes the function return non-zero.

    Components that match nothing in the bundle are reported as a
    ``::warning::`` (stale or platform-scoped — not a failure). License
    texts are resolved relative to ``manifest_dir`` (the repo root), since
    each component ``text`` value is repo-root-relative.

    Returns 0 when no errors were found, non-zero otherwise.
    """
    internal = manifest.get("internal_sonames", [])
    excluded = manifest.get("exclude_not_bundled", [])
    components = manifest.get("components", [])

    errors = []
    matched_components = set()
    checked = 0

    for name, rp in enumerate_assets(staging_root):
        checked += 1

        if is_internal(name, internal):
            continue
        if any(fnmatch.fnmatch(name, g) for g in excluded):
            continue

        rel = os.path.relpath(rp, staging_root)
        comp = match_component(name, components)
        if comp is None:
            errors.append(
                f"::error::{name} ({rel}) is bundled but maps to no license "
                f"component in the manifest"
            )
            continue

        matched_components.add(id(comp))

        text_path = os.path.join(manifest_dir, comp["text"])
        if not os.path.isfile(text_path):
            errors.append(
                f"::error::{name} -> component '{comp['name']}' references "
                f"missing license text '{comp['text']}'"
            )
            continue

        actual = _sha256_file(text_path)
        if actual != comp.get("sha256"):
            errors.append(
                f"::error::{name} -> component '{comp['name']}' license text "
                f"'{comp['text']}' sha256 mismatch (expected "
                f"{comp.get('sha256')}, got {actual})"
            )

    for c in components:
        if id(c) not in matched_components:
            print(
                f"::warning::component '{c['name']}' (match '{c['match']}') "
                f"matched no bundled library (stale or platform-scoped)"
            )

    for line in errors:
        print(line)

    print(
        f"checked {checked} assets, {len(components)} components, "
        f"{len(errors)} errors"
    )
    return 1 if errors else 0


def _load_manifest(path: str):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="check-bundled-licenses",
        description="Bundled-license checker for LibreCelik artifacts.",
    )
    repo_default = os.path.normpath(
        os.path.join(
            os.path.dirname(os.path.abspath(__file__)),
            "..",
            "..",
            "licenses",
            "manifest.json",
        )
    )
    parser.add_argument(
        "--manifest",
        default=repo_default,
        help="Path to the license manifest JSON (default: "
        "licenses/manifest.json relative to the repo).",
    )
    parser.add_argument(
        "--emit-candidates",
        metavar="ROOT",
        help="Enumerate the staging tree at ROOT and print candidate "
        "manifest entries (as pretty JSON) for every unmapped library.",
    )
    parser.add_argument(
        "--check",
        metavar="ROOT",
        help="Fail-closed: verify every bundled library under ROOT maps to "
        "a manifest component with a verbatim license text. Non-zero on "
        "any unmapped library, missing text, or sha256 mismatch.",
    )
    args = parser.parse_args(argv)

    if args.emit_candidates:
        manifest = _load_manifest(args.manifest)
        candidates = emit_candidates(args.emit_candidates, manifest)
        print(json.dumps(candidates, indent=2, ensure_ascii=False))
        return 0

    if args.check:
        manifest = _load_manifest(args.manifest)
        # ``text`` values are repo-root-relative and manifest.json lives at
        # <repo>/licenses/manifest.json, so the repo root used to resolve
        # license texts is the manifest file's parent's parent.
        manifest_dir = os.path.dirname(
            os.path.dirname(os.path.abspath(args.manifest))
        )
        return run_check(args.check, manifest, manifest_dir)

    parser.error("no mode selected (expected --emit-candidates or --check)")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
