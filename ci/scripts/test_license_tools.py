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
