# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""Unit tests for the bundled-license checker tooling.

The checker script has a hyphenated filename, so it cannot be imported
with a normal ``import`` statement; we load it by file path via
``importlib.util``.
"""

import hashlib
import importlib.util
import pathlib

spec = importlib.util.spec_from_file_location(
    "checker", pathlib.Path(__file__).with_name("check-bundled-licenses.py")
)
checker = importlib.util.module_from_spec(spec)
spec.loader.exec_module(checker)

_gspec = importlib.util.spec_from_file_location(
    "gen_notices", pathlib.Path(__file__).with_name("gen-third-party-notices.py")
)
gen = importlib.util.module_from_spec(_gspec)
_gspec.loader.exec_module(gen)


def test_normalize_soname():
    assert checker.normalize("libcurl.so.4.8.0") == "libcurl.so"
    assert checker.normalize("libQt6Core.so.6.10.0") == "libQt6Core.so"
    assert checker.normalize("libicudata.so.74.2") == "libicudata.so"
    assert checker.normalize("libfoo.dylib") == "libfoo.dylib"
    assert checker.normalize("plain-name") == "plain-name"


def test_normalize_macos_version_before_extension():
    """macOS dylibs put the version segment BEFORE the extension
    (``libssl.3.dylib``), unlike Linux sonames which put it AFTER
    (``libssl.so.3``). The checker must collapse both shapes to the same
    bare-extension form so a single manifest entry covers both platforms.
    """
    assert checker.normalize("libssl.3.dylib") == "libssl.dylib"
    assert checker.normalize("libssl.3.0.0.dylib") == "libssl.dylib"
    assert checker.normalize("libcrypto.3.dylib") == "libcrypto.dylib"
    # Multi-component name with version-before-extension.
    assert checker.normalize("libQt6Core.6.dylib") == "libQt6Core.dylib"
    # Don't strip a non-numeric segment — only digits-with-dots is a
    # version, otherwise we'd corrupt names like libfoo.helper.dylib.
    assert checker.normalize("libfoo.helper.dylib") == "libfoo.helper.dylib"
    # Hyphen-version dylibs (e.g. Homebrew openssl@3 packaging) have no
    # numeric .N segment; the hyphenated suffix is part of the bare name.
    assert checker.normalize("libssl-3.dylib") == "libssl-3.dylib"


def test_normalize_requires_extension_boundary():
    # A ".so"/".dylib" substring that is NOT a real extension boundary
    # must not be mis-normalized (would otherwise misattribute a license).
    assert checker.normalize("libfoo.solics") == "libfoo.solics"
    assert checker.normalize("libfoo.so-backup") == "libfoo.so-backup"
    assert checker.normalize("libfoo.dylibext") == "libfoo.dylibext"
    assert checker.normalize("libfoo.so") == "libfoo.so"


def test_enumerate_ignores_non_extension_so_substring(tmp_path):
    lib = tmp_path / "usr" / "lib"
    lib.mkdir(parents=True)
    (lib / "libcurl.so.4").write_bytes(b"")
    (lib / "notes.solar").write_bytes(b"")  # must be ignored
    names = [n for n, _rp in checker.enumerate_assets(str(tmp_path))]
    assert "libcurl.so" in names
    assert all("solar" not in n for n in names)


def test_longest_specific_match_wins():
    comps = [
        {
            "match": "libQt6*.so",
            "name": "Qt 6",
            "spdx": "LGPL-3.0-or-later",
            "text": "a",
        },
        {
            "match": "libQt6VirtualKeyboard.so",
            "name": "QtVK",
            "spdx": "GPL-3.0-only",
            "text": "b",
        },
    ]
    assert checker.match_component("libQt6VirtualKeyboard.so", comps)["name"] == "QtVK"
    assert checker.match_component("libQt6Core.so", comps)["name"] == "Qt 6"
    assert checker.match_component("libcurl.so", comps) is None


def test_internal_skipped():
    assert checker.is_internal(
        "librescrs-pkcs11.so", ["librescrs-pkcs11.so", "*-gui-plugin.so"]
    )
    assert checker.is_internal("piv-gui-plugin.so", ["*-gui-plugin.so"])
    assert not checker.is_internal("libcurl.so", ["*-gui-plugin.so"])


def test_internal_skipped_macos_dylib_mirrors():
    """LibreSCRS-owned modules ship as .dylib on macOS (build-dmg.sh renames
    the .so artifacts). The shipped manifest's internal_sonames must cover
    BOTH extensions so the same plugin isn't reported as a third-party
    dependency just because the bundle layout uses Apple's convention.
    """
    import json
    import pathlib

    manifest_path = (
        pathlib.Path(__file__).resolve().parents[2] / "licenses" / "manifest.json"
    )
    manifest = json.loads(manifest_path.read_text())
    internal = manifest["internal_sonames"]

    # PKCS#11 module and the SmartCard core library both ship from LM.
    assert checker.is_internal("librescrs-pkcs11.dylib", internal)
    assert checker.is_internal("libLibreSCRS_SmartCard.dylib", internal)
    # Plugin family globs must also match the .dylib mirrors.
    assert checker.is_internal("libid-card-plugin.dylib", internal)
    assert checker.is_internal("piv-gui-plugin.dylib", internal)
    assert checker.is_internal("libresign-core.dylib", internal)


def test_enumerate_dedupes_symlinks(tmp_path):
    lib = tmp_path / "usr" / "lib"
    lib.mkdir(parents=True)
    (lib / "libcurl.so.4.8.0").write_bytes(b"")
    (lib / "libcurl.so.4").symlink_to(lib / "libcurl.so.4.8.0")
    names = [n for n, _rp in checker.enumerate_assets(str(tmp_path))]
    assert names.count("libcurl.so") == 1


def test_enumerate_finds_dylib(tmp_path):
    lib = tmp_path / "Contents" / "Frameworks"
    lib.mkdir(parents=True)
    (lib / "libfoo.dylib").write_bytes(b"")
    names = [n for n, _rp in checker.enumerate_assets(str(tmp_path))]
    assert "libfoo.dylib" in names


def test_enumerate_finds_framework_bundle(tmp_path):
    """macdeployqt copies Qt as ``QtCore.framework/Versions/A/QtCore`` —
    the binary inside has no ``.so``/``.dylib`` extension, so the
    enumerator must recognise the surrounding ``.framework`` directory
    and yield the bundle name. Otherwise every bundled Qt module is
    invisible to the bundle check and ships without a license entry.
    """
    fw = tmp_path / "Contents" / "Frameworks" / "QtFoo.framework"
    (fw / "Versions" / "A").mkdir(parents=True)
    (fw / "Versions" / "A" / "QtFoo").write_bytes(b"")
    # Realistic extras inside the framework — must NOT be yielded.
    (fw / "Versions" / "A" / "Resources").mkdir()
    (fw / "Versions" / "A" / "Resources" / "Info.plist").write_text("")

    names = [n for n, _rp in checker.enumerate_assets(str(tmp_path))]
    assert "QtFoo.framework" in names


def test_enumerate_framework_does_not_double_count_inner_dylib(tmp_path):
    """If a framework happens to contain an inner ``.dylib``/``.so`` file
    (rare but possible — e.g. a helper library), enumeration should
    yield ONLY the framework bundle name, not the inner file. Otherwise
    the bundle check would flag the inner helper as an unmapped
    third-party library duplicating the framework entry.
    """
    fw = tmp_path / "Contents" / "Frameworks" / "QtBar.framework"
    (fw / "Versions" / "A").mkdir(parents=True)
    (fw / "Versions" / "A" / "QtBar").write_bytes(b"")
    # An inner shared object inside the framework — must be hidden.
    (fw / "Versions" / "A" / "libhelper.dylib").write_bytes(b"")

    names = [n for n, _rp in checker.enumerate_assets(str(tmp_path))]
    assert "QtBar.framework" in names
    assert "libhelper.dylib" not in names


def test_enumerate_skips_non_libraries(tmp_path):
    lib = tmp_path / "usr" / "lib"
    lib.mkdir(parents=True)
    (lib / "libcurl.so.4.8.0").write_bytes(b"")
    (lib / "README.txt").write_bytes(b"")
    names = [n for n, _rp in checker.enumerate_assets(str(tmp_path))]
    assert names == ["libcurl.so"]


def test_emit_candidates_only_unmapped(tmp_path):
    lib = tmp_path / "usr" / "lib"
    lib.mkdir(parents=True)
    # Truly unmapped third-party libs.
    (lib / "libcurl.so.4.8.0").write_bytes(b"")
    (lib / "libQt6Core.so.6.10.0").write_bytes(b"")
    # Internal (LibreSCRS-owned) — must be skipped.
    (lib / "librescrs-pkcs11.so").write_bytes(b"")
    # Deliberately not bundled — must be skipped.
    (lib / "libpcsclite.so.1").write_bytes(b"")
    # Already mapped in components — must be skipped.
    (lib / "libzstd.so.1.5.5").write_bytes(b"")

    manifest = {
        "internal_sonames": ["librescrs-pkcs11.so"],
        "exclude_not_bundled": ["libpcsclite.so"],
        "components": [
            {
                "match": "libzstd.so",
                "name": "zstd",
                "spdx": "BSD-3-Clause",
                "text": "licenses/bsd-3.txt",
                "sha256": "x",
            }
        ],
    }

    out = checker.emit_candidates(str(tmp_path), manifest)
    names = [c["match"] for c in out]

    assert names == ["libQt6Core.so", "libcurl.so"]  # sorted, deduped
    assert all(
        set(c) == {"match", "name", "spdx", "text", "sha256"} for c in out
    )
    assert all(
        c["name"] == "" and c["spdx"] == "" and c["text"] == "" and c["sha256"] == ""
        for c in out
    )


def _man(tmp_path, sha):
    (tmp_path / "resources" / "licenses").mkdir(parents=True)
    (tmp_path / "resources" / "licenses" / "curl.txt").write_text("CURL LICENSE")
    return {
        "internal_sonames": [],
        "exclude_not_bundled": [],
        "carve_out": [],
        "components": [
            {
                "match": "libcurl.so",
                "name": "curl",
                "spdx": "curl",
                "text": "resources/licenses/curl.txt",
                "sha256": sha,
            }
        ],
    }


def test_check_passes(tmp_path):
    sha = hashlib.sha256(b"CURL LICENSE").hexdigest()
    (tmp_path / "app").mkdir()
    (tmp_path / "app" / "libcurl.so.4").write_bytes(b"")
    assert (
        checker.run_check(str(tmp_path / "app"), _man(tmp_path, sha), str(tmp_path))
        == 0
    )


def test_unmapped_fails(tmp_path):
    sha = hashlib.sha256(b"CURL LICENSE").hexdigest()
    (tmp_path / "app").mkdir()
    (tmp_path / "app" / "libmystery.so.1").write_bytes(b"")
    assert (
        checker.run_check(str(tmp_path / "app"), _man(tmp_path, sha), str(tmp_path))
        != 0
    )


def test_hash_mismatch_fails(tmp_path):
    (tmp_path / "app").mkdir()
    (tmp_path / "app" / "libcurl.so.4").write_bytes(b"")
    assert (
        checker.run_check(
            str(tmp_path / "app"), _man(tmp_path, "deadbeef"), str(tmp_path)
        )
        != 0
    )


def test_internal_and_excluded_skipped(tmp_path):
    sha = hashlib.sha256(b"CURL LICENSE").hexdigest()
    m = _man(tmp_path, sha)
    m["internal_sonames"] = ["lib-int.so"]
    m["exclude_not_bundled"] = ["libGL.so"]
    (tmp_path / "app").mkdir()
    for n in ("libcurl.so.4", "lib-int.so", "libGL.so.1"):
        (tmp_path / "app" / n).write_bytes(b"")
    assert checker.run_check(str(tmp_path / "app"), m, str(tmp_path)) == 0


# --- per-component `platforms` filter ----------------------------------


def test_platforms_linux_only_component_ignored_on_macos():
    """A component tagged ``platforms:["linux"]`` must NOT match when the
    current platform is macOS — even if a matching basename appears in the
    bundle. With ``--platform macos`` the linux-only entry is invisible to
    ``match_component`` so the asset is reported as unmapped.
    """
    linux_only = {
        "match": "libavahi-client.so",
        "name": "Avahi",
        "spdx": "LGPL-2.1-or-later",
        "text": "x",
        "sha256": "y",
        "platforms": ["linux"],
    }
    # macOS: linux-only entry must NOT match.
    assert (
        checker.match_component("libavahi-client.so", [linux_only], "macos")
        is None
    )
    # Linux: linux-only entry MUST match.
    assert (
        checker.match_component("libavahi-client.so", [linux_only], "linux")
        is linux_only
    )


def test_platforms_macos_only_component_ignored_on_linux():
    """Symmetric: a component tagged ``platforms:["macos"]`` must NOT
    match on Linux but MUST match on macOS.
    """
    macos_only = {
        "match": "libfoo.dylib",
        "name": "Foo",
        "spdx": "MIT",
        "text": "x",
        "sha256": "y",
        "platforms": ["macos"],
    }
    assert (
        checker.match_component("libfoo.dylib", [macos_only], "linux") is None
    )
    assert (
        checker.match_component("libfoo.dylib", [macos_only], "macos")
        is macos_only
    )


def test_platforms_absent_matches_all():
    """A component WITHOUT a ``platforms`` key is cross-platform: it must
    match under both ``--platform linux`` and ``--platform macos``.
    """
    cross = {
        "match": "libcurl.so",
        "name": "curl",
        "spdx": "curl",
        "text": "x",
        "sha256": "y",
    }
    assert checker.match_component("libcurl.so", [cross], "linux") is cross
    assert checker.match_component("libcurl.so", [cross], "macos") is cross


def test_platforms_absent_matches_when_no_flag():
    """Backward-compat: when no ``--platform`` arg is passed (current
    platform is ``None``), every component matches regardless of its
    ``platforms`` key. This preserves the legacy fail-closed behavior on
    callers that haven't been updated yet (e.g. the test suite).
    """
    linux_only = {
        "match": "libavahi-client.so",
        "name": "Avahi",
        "spdx": "LGPL-2.1-or-later",
        "text": "x",
        "sha256": "y",
        "platforms": ["linux"],
    }
    macos_only = {
        "match": "libfoo.dylib",
        "name": "Foo",
        "spdx": "MIT",
        "text": "x",
        "sha256": "y",
        "platforms": ["macos"],
    }
    cross = {
        "match": "libcurl.so",
        "name": "curl",
        "spdx": "curl",
        "text": "x",
        "sha256": "y",
    }
    # No platform passed (None) — everything matches.
    assert checker.match_component("libavahi-client.so", [linux_only], None) is linux_only
    assert checker.match_component("libfoo.dylib", [macos_only], None) is macos_only
    assert checker.match_component("libcurl.so", [cross], None) is cross


def test_emit_candidates_respects_platform_filter(tmp_path):
    """A component scoped to a platform OTHER than the current one must
    NOT shield a bundled library from the candidate list. Otherwise a
    Linux-only entry would silently cover a macOS-bundled basename of the
    same soname even though no license text would be emitted for it.
    """
    lib = tmp_path / "app"
    lib.mkdir()
    (lib / "libcurl.so.4").write_bytes(b"")

    # Component matches the asset BUT is scoped to macOS only.
    manifest = {
        "internal_sonames": [],
        "exclude_not_bundled": [],
        "components": [
            {
                "match": "libcurl.so",
                "name": "curl",
                "spdx": "curl",
                "text": "x",
                "sha256": "y",
                "platforms": ["macos"],
            }
        ],
    }

    # On Linux: the macOS-only component does NOT cover libcurl, so it
    # appears as an unmapped candidate.
    out = checker.emit_candidates(str(tmp_path / "app"), manifest, "linux")
    assert [c["match"] for c in out] == ["libcurl.so"]

    # On macOS: the component covers libcurl, so no candidate is emitted.
    out_macos = checker.emit_candidates(str(tmp_path / "app"), manifest, "macos")
    assert out_macos == []


# --- gen-third-party-notices.py ----------------------------------------


def _gen_setup(tmp_path):
    """Two components sharing one MIT text + one with a distinct text."""
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True)
    (lic / "mit.txt").write_text("MIT LICENSE BODY")
    (lic / "zlib.txt").write_text("ZLIB LICENSE BODY")
    components = [
        {"name": "libfoo", "spdx": "MIT", "text": "resources/licenses/mit.txt"},
        {"name": "libbar", "spdx": "MIT", "text": "resources/licenses/mit.txt"},
        {"name": "zlib", "spdx": "Zlib", "text": "resources/licenses/zlib.txt"},
    ]
    return components, {"lc": str(tmp_path)}


def test_render_lists_all_names_and_dedupes_body(tmp_path):
    components, base_dirs = _gen_setup(tmp_path)
    out = gen.render(components, base_dirs)
    # Both names that share the MIT text appear.
    assert "libfoo" in out
    assert "libbar" in out
    assert "zlib" in out
    # Shared MIT body emitted exactly once.
    assert out.count("MIT LICENSE BODY") == 1
    # Distinct body present.
    assert "ZLIB LICENSE BODY" in out


def test_render_deterministic(tmp_path):
    components, base_dirs = _gen_setup(tmp_path)
    assert gen.render(components, base_dirs) == gen.render(components, base_dirs)


def test_render_sorted_case_insensitive(tmp_path):
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True)
    (lic / "a.txt").write_text("A")
    (lic / "z.txt").write_text("Z")
    components = [
        {"name": "Zebra", "spdx": "MIT", "text": "resources/licenses/z.txt"},
        {"name": "apple", "spdx": "MIT", "text": "resources/licenses/a.txt"},
    ]
    out = gen.render(components, {"lc": str(tmp_path)})
    # "apple" section header precedes "Zebra" (case-insensitive name sort).
    assert out.index("apple") < out.index("Zebra")


def test_render_respects_platform_filter(tmp_path):
    """The Tier-2 notice baked into the binary must list ONLY components that
    actually ship in the current platform's artifact. A linux-only entry must
    be omitted when the build's ``--platform`` is ``macos`` and vice versa;
    cross-platform entries (no ``platforms`` key) always render.

    This mirrors the checker's :func:`_component_applies_to_platform` rule so
    a single manifest drives both verification AND the rendered notice.
    """
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True)
    (lic / "mit.txt").write_text("MIT LICENSE BODY")
    (lic / "lgpl.txt").write_text("LGPL LICENSE BODY")
    (lic / "bsd.txt").write_text("BSD LICENSE BODY")
    components = [
        {
            "name": "libcross",
            "spdx": "MIT",
            "text": "resources/licenses/mit.txt",
        },
        {
            "name": "libavahi-client",
            "spdx": "LGPL-2.1-or-later",
            "text": "resources/licenses/lgpl.txt",
            "platforms": ["linux"],
        },
        {
            "name": "libSecurityFoundation",
            "spdx": "APSL-2.0",
            "text": "resources/licenses/bsd.txt",
            "platforms": ["macos"],
        },
    ]
    base_dirs = {"lc": str(tmp_path)}

    # Render the macOS notice: filter out components whose platforms list
    # excludes "macos", then render the remainder.
    filtered = [
        c for c in components
        if gen._component_applies_to_platform(c, "macos")
    ]
    out = gen.render(filtered, base_dirs)

    assert "libcross" in out
    assert "libSecurityFoundation" in out
    assert "libavahi-client" not in out

    # Symmetric direction: filtering for linux must drop the macOS-only
    # entry — guards against a typo like ``current_platform == "macos"``
    # being hardcoded instead of using the membership check.
    filtered_linux = [
        c for c in components
        if gen._component_applies_to_platform(c, "linux")
    ]
    out_linux = gen.render(filtered_linux, base_dirs)
    assert "libcross" in out_linux
    assert "libavahi-client" in out_linux
    assert "libSecurityFoundation" not in out_linux


def test_gen_main_end_to_end_platform_filter(tmp_path):
    """End-to-end ``main()`` invocation with ``--platform macos`` must drive
    the filter through argparse → load → render → file write. Guards against
    a future refactor that drops the filter step from main() while keeping
    the helper intact (the helper-only test above would still pass).
    """
    # gen.main() resolves the LC repo root as dirname(dirname(manifest)),
    # so the manifest must live two levels below the asset tree root.
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True)
    (lic / "mit.txt").write_text("MIT LICENSE BODY")
    (lic / "lgpl.txt").write_text("LGPL LICENSE BODY")
    manifest_dir = tmp_path / "licenses"
    manifest_dir.mkdir()
    manifest = manifest_dir / "manifest.json"
    import json
    manifest.write_text(json.dumps({
        "components": [
            {"name": "libcross", "spdx": "MIT",
             "text": "resources/licenses/mit.txt"},
            {"name": "libavahi-client", "spdx": "LGPL-2.1-or-later",
             "text": "resources/licenses/lgpl.txt",
             "platforms": ["linux"]},
        ]
    }))
    out_file = tmp_path / "THIRD-PARTY-LICENSES.txt"

    rc = gen.main([
        "--manifest", str(manifest),
        "-o", str(out_file),
        "--platform", "macos",
    ])
    assert rc == 0
    text = out_file.read_text()
    assert "libcross" in text
    assert "libavahi-client" not in text
