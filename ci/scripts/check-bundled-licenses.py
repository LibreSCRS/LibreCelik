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

# macOS convention puts the version BEFORE the extension:
# ``libssl.3.dylib`` / ``libssl.3.0.0.dylib``. Collapse those to the
# bare ``libssl.dylib`` form so a single manifest entry covers the
# library regardless of how the version segment is spelled. Only digits
# (with dots between) are considered a version — a non-numeric segment
# (e.g. ``libfoo.helper.dylib``) is a real part of the name and is left
# untouched. Linux-style ``.so.<N>`` is handled separately by ``_EXT``.
_DYLIB_VERSIONED = re.compile(r"^(lib[^.]+)(?:\.\d+)+\.dylib$")


def normalize(basename: str) -> str:
    """Strip version segments around the ``.so``/``.dylib`` extension.

    Handles both conventions:

    - Linux soname: version AFTER the extension (``libcurl.so.4.8.0``).
    - macOS dylib: version BEFORE the extension (``libssl.3.dylib``).

    Examples::

        libcurl.so.4.8.0     -> libcurl.so
        libQt6Core.so.6.10.0 -> libQt6Core.so
        libicudata.so.74.2   -> libicudata.so
        libssl.3.dylib       -> libssl.dylib
        libssl.3.0.0.dylib   -> libssl.dylib
        libfoo.dylib         -> libfoo.dylib
        libfoo.helper.dylib  -> libfoo.helper.dylib (non-numeric segment)
        libfoo.solics        -> libfoo.solics (not a real .so boundary)
        plain-name           -> plain-name (no extension, unchanged)
    """
    # macOS version-before-extension form. Check this first because the
    # name still ends in ``.dylib`` (a valid _EXT match that would
    # otherwise leave the embedded version segments in place).
    m = _DYLIB_VERSIONED.match(basename)
    if m:
        return m.group(1) + ".dylib"

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


def _component_applies_to_platform(comp, current_platform) -> bool:
    """Return True if ``comp`` is in scope for ``current_platform``.

    Filtering rules:

    - A component WITHOUT a ``platforms`` key is cross-platform and
      applies on every platform (this is the default — keep most
      entries unscoped so a single record covers both Linux and macOS).
    - A component WITH a ``platforms`` list applies only when
      ``current_platform`` is in that list.
    - When ``current_platform`` is ``None`` (no ``--platform`` flag was
      passed) every component applies regardless of its ``platforms``
      key. This preserves the legacy fail-closed behavior for callers
      that have not been updated to pass an explicit platform.
    """
    platforms = comp.get("platforms")
    if platforms is None:
        return True
    if current_platform is None:
        return True
    return current_platform in platforms


def match_component(name: str, components, current_platform=None):
    """Return the most-specific component matching ``name``, else None.

    Globs are matched case-sensitively against the whole normalized
    basename. When several entries match, the one whose ``match`` pattern
    has the most non-wildcard characters (the most specific) wins, so a
    literal soname always beats a family glob.

    When ``current_platform`` is supplied, components whose ``platforms``
    list excludes it are invisible to the match: a Linux-only entry
    cannot satisfy a macOS bundle and vice versa. See
    :func:`_component_applies_to_platform` for the full rule set.
    """
    best = None
    best_specificity = -1
    for c in components:
        if not _component_applies_to_platform(c, current_platform):
            continue
        if fnmatch.fnmatchcase(name, c["match"]):
            specificity = len(c["match"].replace("*", "").replace("?", ""))
            if specificity > best_specificity:
                best = c
                best_specificity = specificity
    return best


def _is_framework_dir(name: str) -> bool:
    """True if ``name`` is the basename of a macOS framework bundle."""
    return name.endswith(".framework")


def enumerate_assets(root: str):
    """Yield ``(normalized_name, realpath)`` for every bundled artefact.

    Walks the staging tree under ``root`` and yields each artefact
    exactly once, deduped by ``os.path.realpath`` so a symlink and its
    target are counted a single time.

    Two kinds of artefacts are emitted:

    - Plain shared libraries (``*.so*``/``*.dylib*``) — yielded with
      their normalized basename (``libcurl.so``, ``libssl.dylib``).
    - macOS framework bundles (``QtCore.framework`` under e.g.
      ``Contents/Frameworks/``) — yielded with the bundle directory's
      basename (``QtCore.framework``). The inner files (binary at
      ``Versions/A/QtCore``, helper ``.dylib``s, ``Resources/``) are
      NOT also yielded, so a framework counts as a single licensable
      unit rather than being double-counted by its internals.
    """
    seen = set()
    for dirpath, dirs, files in os.walk(root):
        # When the current directory is itself a framework bundle, emit
        # the bundle once and stop descending — every inner file belongs
        # to the framework and must not be enumerated separately.
        if _is_framework_dir(os.path.basename(dirpath)):
            rp = os.path.realpath(dirpath)
            if rp not in seen:
                seen.add(rp)
                yield os.path.basename(dirpath), rp
            dirs[:] = []
            continue

        for f in files:
            if not _EXT.search(f):
                continue
            full = os.path.join(dirpath, f)
            rp = os.path.realpath(full)
            if rp in seen:
                continue
            seen.add(rp)
            yield normalize(f), rp


def emit_candidates(root: str, manifest, current_platform=None):
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

    ``current_platform`` is forwarded to :func:`match_component` so that
    a platform-scoped component does NOT shield a bundled asset on a
    different platform — otherwise a Linux-only entry would silently
    cover a macOS-bundled soname of the same name while no license text
    would actually ship for it.
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
        if match_component(name, components, current_platform) is not None:
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


def run_check(
    staging_root: str, manifest, manifest_dir: str, current_platform=None
) -> int:
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

    ``current_platform`` selects which components are considered in
    scope. A component whose ``platforms`` list excludes the current
    platform is treated as if it weren't in the manifest at all — it
    cannot satisfy an asset on the wrong platform, and the stale-entry
    warning is suppressed (since "not matched on macOS because it's
    Linux-only" is by design, not stale). See
    :func:`_component_applies_to_platform` for the full rule set.

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
        comp = match_component(name, components, current_platform)
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
        # Skip the stale-entry warning for components scoped to a
        # different platform: an Avahi entry that doesn't match anything
        # under `--platform macos` is correct, not stale.
        if not _component_applies_to_platform(c, current_platform):
            continue
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
    parser.add_argument(
        "--platform",
        choices=("linux", "macos"),
        default=None,
        help="Scope the check/candidate enumeration to a single platform. "
        "Components with a ``platforms`` list apply only when the current "
        "platform is in that list; components without the key are "
        "cross-platform. Omit the flag to disable platform filtering "
        "(legacy behavior: every component applies).",
    )
    args = parser.parse_args(argv)

    if args.emit_candidates:
        manifest = _load_manifest(args.manifest)
        candidates = emit_candidates(
            args.emit_candidates, manifest, args.platform
        )
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
        return run_check(args.check, manifest, manifest_dir, args.platform)

    parser.error("no mode selected (expected --emit-candidates or --check)")


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
