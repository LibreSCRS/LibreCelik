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
import plistlib
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

# The mirror images of the two patterns above: what ``normalize`` throws
# away is exactly the version a bill of materials needs. Read from the
# file the tree actually holds — never typed anywhere — so the bill
# cannot disagree with the artefact it describes.
#
# This is the SONAME version, not the upstream release: linuxdeploy stages
# libcurl as ``libcurl.so.4``, so the bill says curl 4. That is honest and
# coarse, which is why every component also carries the sha256 of the
# bundled file: the hash identifies the exact object shipped even when the
# soname only names an ABI generation.
_SO_VERSION = re.compile(r"\.so\.([0-9][0-9A-Za-z.\-]*)$")
_DYLIB_VERSION = re.compile(r"^lib[^.]+\.((?:\d+)(?:\.\d+)*)\.dylib$")


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
      ``current_platform`` is in that list, and never when the caller
      named no platform at all. ``--platform`` is required at the command
      line, so ``None`` reaches here only from a direct helper call.
    """
    platforms = comp.get("platforms")
    if platforms is None:
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


def soname_version(basename: str) -> str:
    """Version as the bundled file itself spells it, or "" if it does not.

    Examples::

        libcrypto.so.3        -> "3"
        libQt6Core.so.6.10.0  -> "6.10.0"
        libssl.3.0.0.dylib    -> "3.0.0"
        libqxcb.so            -> ""
    """
    m = _SO_VERSION.search(basename)
    if m:
        return m.group(1)
    m = _DYLIB_VERSION.match(basename)
    if m:
        return m.group(1)
    return ""


def framework_version(path: str) -> str:
    """Version of a macOS framework bundle, from its Info.plist, or "".

    A framework carries no version in its name, so the only place it is
    written down is the bundle's own metadata. Both plist encodings are
    handled by ``plistlib``. A framework without a readable plist yields
    "" rather than an exception: a missing version must weaken the bill,
    not abort the build.
    """
    for rel in ("Resources/Info.plist", "Versions/Current/Resources/Info.plist"):
        p = os.path.join(path, rel)
        if not os.path.isfile(p):
            continue
        try:
            with open(p, "rb") as fh:
                d = plistlib.load(fh)
        except Exception:
            return ""
        return str(
            d.get("CFBundleShortVersionString") or d.get("CFBundleVersion") or ""
        )
    return ""


def _framework_digest(path: str) -> str:
    """sha256 of a framework bundle's Mach-O binary, or "" if unreadable.

    :func:`enumerate_assets` yields a framework as its BUNDLE DIRECTORY, so
    the plain-file hash helper cannot be pointed at it: ``open()`` on a
    directory raises ``IsADirectoryError``. The object that actually ships
    is the binary inside, which is where the hash belongs.
    """
    stem = os.path.basename(path)
    if stem.endswith(".framework"):
        stem = stem[: -len(".framework")]
    for cand in (
        os.path.join(path, "Versions", "Current", stem),
        os.path.join(path, stem),
    ):
        if os.path.isfile(cand):
            return _sha256_file(cand)
    return ""


def licence_entry(spdx: str):
    """One CycloneDX licence value in the shape the string requires.

    CycloneDX takes three different shapes and the manifest holds all
    three. Putting a compound expression into ``license.id`` produces a
    document no consumer will parse, which would be the same failure this
    whole mode exists to end — a bill that looks authoritative and says
    nothing usable.

    Measured over licenses/manifest.json: 23 distinct spdx values, of
    which three are not plain ids — "IJG AND BSD-3-Clause AND Zlib",
    "BSD-3-Clause OR GPL-2.0-only", "GPL-3.0-only WITH GCC-exception-3.1"
    (expressions) — and one is a LicenseRef
    ("LicenseRef-libselinux-public-domain").
    """
    if " AND " in spdx or " OR " in spdx or " WITH " in spdx:
        return {"expression": spdx}
    if spdx.startswith("LicenseRef-"):
        return {"license": {"name": spdx}}
    return {"license": {"id": spdx}}


def _soname_stem(soname: str) -> str:
    """The soname with its shared-library extension removed, nothing else."""
    for suffix in (".so", ".dylib", ".framework"):
        if soname.endswith(suffix):
            return soname[: -len(suffix)]
    return soname


def purl_for(comp, soname: str, version: str) -> str:
    """Package URL for one bundled object.

    A manifest component may carry an explicit ``purl`` key; that is the
    one a vulnerability database will actually match against, and it wins.
    Without one the purl is derived from the soname, which is honest but
    coarse: libcrypto.so.3 derives "pkg:generic/crypto@3", not
    "pkg:generic/openssl@3". The security-relevant components carry an
    explicit key for exactly that reason; the rest are derived.
    """
    explicit = comp.get("purl") if comp else None
    if explicit:
        if version and "@" not in explicit:
            return explicit + "@" + version
        return explicit
    stem = _soname_stem(soname)
    # The ecosystem convention: libcurl.so IS the package "curl". It holds
    # for third-party sonames and only for those -- see the internal branch
    # of emit_sbom, where the same strip would eat three letters of our own
    # project's name.
    if stem.startswith("lib"):
        stem = stem[3:]
    return "pkg:generic/%s%s" % (stem, "@" + version if version else "")


def emit_sbom(
    root: str, manifest, current_platform, app_name: str, app_version: str
):
    """CycloneDX bill of materials for one packaging staging tree.

    Returns ``(document, 0)`` or ``(None, non-zero)``.

    Same walk, same matching and the same fail-closed rule as
    :func:`run_check`: what the artefact carries is what the bill lists.
    Deriving the bill from the tree rather than from a hand-written list
    of source pins is the entire point. A list of pins is a claim, and a
    claim is what went wrong: the published, cosign-signed bill beside
    this repository's AppImage carried ``"components": []`` for a bundle
    that ships OpenSSL, Qt and curl, because not one of the source paths
    its pin reader searched exists in this repository. It printed
    "0 components" and exited 0.

    Three refusals, all of them things that producer did silently:

    - an empty staging tree (``checked == 0``) -> rc 1;
    - a bundled object that maps to no manifest component -> rc 1, the
      same fail-closed rule the licence check applies, so the bill can
      never be quietly shorter than the bundle;
    - an empty component list -> rc 2. A bill with no components is not a
      small bill. It is a signed document stating that this artefact
      bundles no third-party code.

    First-party objects (``internal_sonames``) are listed too, without a
    licence: a consumer asking "which build is inside" is asking a
    bill-of-materials question, and the several first-party globs do not
    share one licence, so naming one here would be a guess.

    One deliberate difference from :func:`run_check`: the exclusion globs
    are consulted BEFORE the internal ones here, and after them there.
    Over today's manifest the two sets do not intersect, so the two modes
    agree; if a glob is ever added to both, they would not, and this is
    the line to change.
    """
    internal = manifest.get("internal_sonames", [])
    excluded = manifest.get("exclude_not_bundled", [])
    components = manifest.get("components", [])

    # enumerate_assets yields realpaths so a symlink and its target count
    # once; the root every bom-ref is measured against has to be resolved
    # the same way or the relative path walks upward out of the tree. On
    # macOS that is the normal case, not a corner one: the packaging script
    # stages under mktemp -d, which returns a path below /var/folders, and
    # /var is a symlink to /private/var -- every path in the published,
    # signed bill would have been the build host's temporary directory.
    root = os.path.realpath(root)

    errors = []
    entries = []
    checked = 0
    unversioned = 0

    for name, rp in enumerate_assets(root):
        checked += 1
        rel = os.path.relpath(rp, root)
        real = os.path.basename(rp)

        if any(fnmatch.fnmatch(name, g) for g in excluded):
            continue

        if name.endswith(".framework"):
            version = framework_version(rp)
        else:
            version = soname_version(real)

        if is_internal(name, internal):
            # A first-party object is named by its soname, minus only the
            # extension. purl_for's "lib" strip is the convention that
            # libcurl.so is the package "curl"; over our own names it eats
            # three letters of the project -- librescrs-pkcs11.so would be
            # billed as "rescrs-pkcs11", libresign.so as "resign", and both
            # of those go into a signed document nobody re-reads.
            comp_name = _soname_stem(name)
            licence = None
            props = [{"name": "librescrs:internal", "value": "true"}]
            version = version or app_version
            purl = "pkg:generic/%s%s" % (
                comp_name, "@" + version if version else ""
            )
        else:
            comp = match_component(name, components, current_platform)
            if comp is None:
                errors.append(
                    f"::error::{name} ({rel}) is bundled but maps to no "
                    f"license component in the manifest"
                )
                continue
            comp_name = comp["name"]
            licence = licence_entry(comp["spdx"])
            props = []
            purl = purl_for(comp, name, version)

        if not version:
            unversioned += 1

        # A framework is yielded as its bundle DIRECTORY, so the plain-file
        # helper cannot be pointed at it; the binary inside is the object
        # that ships. An unreadable one weakens the entry rather than
        # aborting the build, which is why the hash list is left empty
        # rather than carrying a digest of nothing that would read as a
        # real one. The key itself stays: CycloneDX 1.5 puts no minimum on
        # the array, and a consumer testing for an empty list is testing
        # the thing that is actually true.
        digest = _sha256_file(rp) if os.path.isfile(rp) else _framework_digest(rp)

        entry = {
            "bom-ref": rel,
            "type": "library",
            "name": comp_name,
            "version": version or "unknown",
            "purl": purl,
            "hashes": (
                [{"alg": "SHA-256", "content": digest}] if digest else []
            ),
            "properties": props
            + [
                {"name": "librescrs:soname", "value": real},
                {"name": "librescrs:path", "value": rel},
            ],
        }
        if licence is not None:
            entry["licenses"] = [licence]
        entries.append(entry)

    if checked == 0:
        errors.append(
            f"::error::no shared objects found under {root} — the staging "
            f"tree is empty or the path is wrong"
        )

    for line in errors:
        print(line)
    if errors:
        return None, 1

    if not entries:
        print(
            "::error::the bill of materials would be empty — refusing to "
            "write a document that claims this artefact bundles nothing"
        )
        return None, 2

    entries.sort(key=lambda e: (e["name"].lower(), e["bom-ref"]))

    doc = {
        "bomFormat": "CycloneDX",
        "specVersion": "1.5",
        "version": 1,
        "metadata": {
            "component": {
                "type": "application",
                "name": app_name,
                "version": app_version,
            },
            "properties": [
                {"name": "librescrs:platform", "value": current_platform},
                {"name": "librescrs:objects-walked", "value": str(checked)},
            ],
        },
        "components": entries,
    }
    print(
        "sbom: %d components from %d bundled objects (%d without a version)"
        % (len(entries), checked, unversioned)
    )
    return doc, 0


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

    # Same reason as in emit_sbom: the assets come back resolved, so the
    # root they are made relative to has to be resolved too, or every path
    # in an ::error:: line points outside the tree the reader is looking at.
    staging_root = os.path.realpath(staging_root)

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

    # A staging tree with no shared objects at all is not a pass. Every
    # artefact this gate guards carries Qt; "checked 0 assets" means the
    # caller pointed at the wrong directory, or packaging moved and this
    # gate now guards nothing. A fail-closed check that returns 0 over an
    # empty tree is not fail-closed, it is a vacuum: an empty directory
    # used to produce "checked 0 assets, 149 components, 0 errors" and
    # rc=0. The stale-entry survey is skipped in that case -- it would
    # print one warning per manifest entry and bury the one line that
    # matters.
    if checked == 0:
        errors.append(
            f"::error::no shared objects found under {staging_root} — the "
            f"staging tree is empty or the path is wrong"
        )
    else:
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
        "--sbom",
        metavar="ROOT",
        help="Write a CycloneDX bill of materials for the staging tree at "
        "ROOT to --sbom-out. Fail-closed: non-zero on an empty tree, an "
        "unmapped library, or a bill that would carry no components.",
    )
    parser.add_argument(
        "--sbom-out",
        metavar="PATH",
        help="Where --sbom writes the document. Required with --sbom.",
    )
    parser.add_argument(
        "--app-name",
        metavar="NAME",
        default=None,
        help="Product name recorded as metadata.component.name. Defaults "
        "to the name of the directory holding the manifest's parent, "
        "which is the checkout path -- true in CI and a guess anywhere "
        "else. Every packaging caller in this tree passes it.",
    )
    parser.add_argument(
        "--app-version",
        metavar="VERSION",
        help="Version recorded as metadata.component.version. Required "
        "with --sbom: the caller already resolved the version it stamped "
        "on the artefact file name, and a bill that disagrees with the "
        "artefact beside it is the drift this whole mode exists to end.",
    )
    parser.add_argument(
        "--platform",
        choices=("linux", "macos"),
        required=True,
        help="Scope the check/candidate enumeration to a single platform. "
        "Components with a ``platforms`` list apply only when the current "
        "platform is in that list; components without the key are "
        "cross-platform. Required: applying every component regardless of "
        "platform answered a question nobody asked, and every caller in "
        "this tree already knows which artifact it is building.",
    )
    args = parser.parse_args(argv)

    modes = [
        flag
        for flag, given in (
            ("--emit-candidates", args.emit_candidates),
            ("--check", args.check),
            ("--sbom", args.sbom),
        )
        if given
    ]
    if len(modes) > 1:
        # The dispatch below returns from the first mode it matches, so a
        # second one was discarded in silence -- and the one that loses is
        # whichever comes later here, which is --sbom. A caller folding the
        # two adjacent packaging invocations into one command would have got
        # a licence verdict, rc 0, and no bill written at all. The two walks
        # are two steps on purpose; say so instead of dropping one.
        parser.error(
            "%s are separate modes; run them as separate invocations"
            % ", ".join(modes)
        )

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

    if args.sbom:
        if not args.sbom_out:
            parser.error("--sbom requires --sbom-out")
        if not args.app_version:
            parser.error("--sbom requires --app-version")
        manifest = _load_manifest(args.manifest)
        repo_root = os.path.dirname(
            os.path.dirname(os.path.abspath(args.manifest))
        )
        doc, rc = emit_sbom(
            args.sbom,
            manifest,
            args.platform,
            args.app_name or os.path.basename(repo_root),
            args.app_version,
        )
        if rc != 0:
            return rc
        # Written only after the document is complete and non-empty: a
        # half-written bill on disk is a bill someone will sign.
        with open(args.sbom_out, "w", encoding="utf-8") as fh:
            json.dump(doc, fh, indent=2, ensure_ascii=False)
            fh.write("\n")
        print(f"sbom: wrote {args.sbom_out}")
        return 0

    parser.error(
        "no mode selected (expected --emit-candidates, --check or --sbom)"
    )


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
