#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""Build-time third-party license notice generator for LibreCelik.

Reads one or more license manifests (the LibreCelik manifest and,
optionally, the LibreMiddleware ``thirdparty`` manifest) and renders a
single deterministic ``THIRD-PARTY-LICENSES.txt`` that lists the license
of every bundled third-party component.

Components are grouped by the *resolved* license-text file: dozens of
MIT-licensed libraries that all point at the same ``mit.txt`` collapse
into one section whose header enumerates every (name, SPDX) pair using
that text, followed by the verbatim license body emitted once.

The output is byte-for-byte deterministic for a given input so it can be
embedded as a Qt resource without churning the build.
"""

import argparse
import json
import os
import sys

_RULE = "=" * 64

_HEADER = (
    _RULE
    + "\n"
    + "THIRD-PARTY SOFTWARE LICENSES\n"
    + _RULE
    + "\n"
    + "This product bundles third-party software components. The licenses\n"
    + "of those components are reproduced verbatim below. Components that\n"
    + "share the same license text are grouped into a single section whose\n"
    + "header lists every component (and its SPDX identifier) covered by\n"
    + "that text.\n"
    + "\n"
)


def _resolve_text_path(component, base_dirs):
    """Resolve a component's ``text`` against its source base directory.

    Each component may carry a ``_base`` tag selecting which entry of
    ``base_dirs`` its ``text`` path is relative to; absent a tag the
    ``"lc"`` base is used.
    """
    base = base_dirs[component.get("_base", "lc")]
    return os.path.normpath(os.path.join(base, component["text"]))


def render(components, base_dirs):
    """Render the deterministic third-party notice text.

    ``components`` is a list of ``{name, spdx, text, ...}`` dicts.
    ``base_dirs`` maps a source tag to the directory each tagged
    component's ``text`` path resolves against.

    Components are grouped by resolved text-file path. Within a group the
    member (name, spdx) pairs are listed sorted case-insensitively by
    name, and groups are ordered by their first (lowest) member name.
    """
    # Group components by resolved text path, preserving each group's
    # members. We read each body once per distinct resolved path.
    groups = {}  # resolved_path -> list[(name, spdx)]
    for c in components:
        path = _resolve_text_path(c, base_dirs)
        groups.setdefault(path, []).append((c["name"], c.get("spdx", "")))

    # Sort members within each group, then order groups by their first
    # member's (case-insensitive) name for a stable, name-driven layout.
    rendered_groups = []
    for path, members in groups.items():
        # Collapse identical (name, spdx) pairs (different sonames of the
        # same project map to one manifest entry name).
        members = sorted(set(members), key=lambda m: (m[0].lower(), m[1]))
        rendered_groups.append((members, path))
    rendered_groups.sort(key=lambda g: (g[0][0][0].lower(), g[0][0][1]))

    parts = [_HEADER]
    for members, path in rendered_groups:
        with open(path, "r", encoding="utf-8") as fh:
            body = fh.read()
        parts.append(_RULE + "\n")
        for name, spdx in members:
            if spdx:
                parts.append(f"{name}  —  {spdx}\n")
            else:
                parts.append(f"{name}\n")
        parts.append(_RULE + "\n")
        parts.append(body)
        if not body.endswith("\n"):
            parts.append("\n")
        parts.append("\n")

    return "".join(parts)


def _load_components(manifest_path, base_tag):
    """Load a manifest's components, tagging each with ``base_tag``."""
    with open(manifest_path, "r", encoding="utf-8") as fh:
        data = json.load(fh)
    components = data["components"] if isinstance(data, dict) else data
    tagged = []
    for c in components:
        # Skip placeholder/incomplete entries with no license text.
        if not c.get("text") or not c.get("name"):
            continue
        c = dict(c)
        c["_base"] = base_tag
        tagged.append(c)
    return tagged


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="gen-third-party-notices",
        description="Generate the bundled third-party license notice.",
    )
    parser.add_argument(
        "--manifest",
        required=True,
        help="LibreCelik license manifest JSON (text paths resolve "
        "against the LC repo root).",
    )
    parser.add_argument(
        "--lm-manifest",
        help="Optional LibreMiddleware thirdparty manifest JSON (text "
        "paths resolve against the LM thirdparty/ directory).",
    )
    parser.add_argument(
        "-o",
        "--output",
        required=True,
        help="Path to write the generated notice file.",
    )
    args = parser.parse_args(argv)

    # LC text paths are repo-root-relative; manifest.json lives at
    # <repo>/licenses/manifest.json, so the root is its parent's parent.
    lc_root = os.path.dirname(os.path.dirname(os.path.abspath(args.manifest)))
    base_dirs = {"lc": lc_root}
    components = _load_components(args.manifest, "lc")

    if args.lm_manifest and os.path.isfile(args.lm_manifest):
        # LM text paths are relative to LM's thirdparty/ directory, which
        # is the manifest file's own parent directory.
        lm_dir = os.path.dirname(os.path.abspath(args.lm_manifest))
        base_dirs["lm"] = lm_dir
        components += _load_components(args.lm_manifest, "lm")

    text = render(components, base_dirs)

    out_dir = os.path.dirname(os.path.abspath(args.output))
    if out_dir:
        os.makedirs(out_dir, exist_ok=True)
    with open(args.output, "w", encoding="utf-8") as fh:
        fh.write(text)
    return 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
