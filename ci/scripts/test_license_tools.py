# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""Unit tests for the bundled-licence tooling: check-bundled-licenses.py
(the fail-closed licence walk and the bill of materials) and
gen-third-party-notices.py. Both have hyphenated names, so they are loaded
by path. One test per behaviour; each assertion names a failure that was
real or that the walk exists to prevent.
"""

import hashlib
import importlib.util
import json
import pathlib
import plistlib

import pytest


def _load(name, filename):
    spec = importlib.util.spec_from_file_location(
        name, pathlib.Path(__file__).with_name(filename)
    )
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


checker = _load("checker", "check-bundled-licenses.py")
gen = _load("gen_notices", "gen-third-party-notices.py")

LICENCE = b"CURL LICENSE"
LICENCE_SHA = hashlib.sha256(LICENCE).hexdigest()
JASPER_SHA = "63e106c80eb72af9fd4fa28772499ab0138b9994"


def _comp(match, name="x", spdx="MIT", platforms=None, **extra):
    c = {"match": match, "name": name, "spdx": spdx, "text": "t", "sha256": "s"}
    if platforms is not None:
        c["platforms"] = platforms
    c.update(extra)
    return c


def _man(tmp_path, sha=LICENCE_SHA):
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True, exist_ok=True)
    (lic / "curl.txt").write_bytes(LICENCE)
    text = "resources/licenses/curl.txt"
    return {
        "internal_sonames": ["libLibreSCRS_*.so"],
        "exclude_not_bundled": ["libpcsclite.so"],
        "carve_out": [],
        "components": [
            {"match": "libcurl.so", "name": "curl", "spdx": "curl",
             "text": text, "sha256": sha},
            {"match": "libjpeg.so", "name": "libjpeg-turbo",
             "spdx": "IJG AND BSD-3-Clause AND Zlib", "text": text,
             "sha256": LICENCE_SHA},
            {"match": "libselinux.so", "name": "libselinux",
             "spdx": "LicenseRef-libselinux-public-domain", "text": text,
             "sha256": LICENCE_SHA},
            {"match": "libjasper.so", "name": "JasPer", "spdx": "JasPer-2.0",
             "text": text, "sha256": LICENCE_SHA, "pin": "jasper"},
        ],
    }


def _staged(tmp_path, names):
    app = tmp_path / "app"
    app.mkdir(exist_ok=True)
    for n in names:
        (app / n).write_bytes(b"x")
    return str(app)


def _names(root):
    return [n for n, _rp in checker.enumerate_assets(str(root))]


# --- names -----------------------------------------------------------------


@pytest.mark.parametrize("raw, bare", [
    ("libcurl.so.4.8.0", "libcurl.so"),
    ("libQt6Core.so.6.10.0", "libQt6Core.so"),
    ("libfoo.so", "libfoo.so"),
    ("plain-name", "plain-name"),
    # macOS puts the version BEFORE the extension; both shapes collapse.
    ("libssl.3.dylib", "libssl.dylib"),
    ("libssl.3.0.0.dylib", "libssl.dylib"),
    ("libQt6Core.6.dylib", "libQt6Core.dylib"),
    # Only digits-and-dots are a version.
    ("libfoo.helper.dylib", "libfoo.helper.dylib"),
    ("libssl-3.dylib", "libssl-3.dylib"),
    # A ".so"/".dylib" substring that is not an extension boundary.
    ("libfoo.solics", "libfoo.solics"),
    ("libfoo.so-backup", "libfoo.so-backup"),
    ("libfoo.dylibext", "libfoo.dylibext"),
])
def test_normalize(raw, bare):
    assert checker.normalize(raw) == bare


def test_enumerate_walks_libraries_and_bundles_only(tmp_path):
    lib = tmp_path / "usr" / "lib"
    lib.mkdir(parents=True)
    (lib / "libcurl.so.4.8.0").write_bytes(b"")
    (lib / "libcurl.so.4").symlink_to(lib / "libcurl.so.4.8.0")  # counted once
    (lib / "notes.solar").write_bytes(b"")
    (lib / "README.txt").write_bytes(b"")
    (lib / "libfoo.dylib").write_bytes(b"")
    # macdeployqt's Qt: the binary has no extension, the bundle is the name,
    # and a helper library inside the bundle is not a second asset.
    fw = tmp_path / "Contents" / "Frameworks" / "QtBar.framework"
    (fw / "Versions" / "A" / "Resources").mkdir(parents=True)
    (fw / "Versions" / "A" / "QtBar").write_bytes(b"")
    (fw / "Versions" / "A" / "libhelper.dylib").write_bytes(b"")
    (fw / "Versions" / "A" / "Resources" / "Info.plist").write_text("")
    assert sorted(_names(tmp_path)) == ["QtBar.framework", "libcurl.so", "libfoo.dylib"]


# --- matching --------------------------------------------------------------


def test_longest_specific_match_wins():
    comps = [_comp("libQt6*.so", "Qt 6"), _comp("libQt6VirtualKeyboard.so", "QtVK")]
    assert checker.match_component("libQt6VirtualKeyboard.so", comps)["name"] == "QtVK"
    assert checker.match_component("libQt6Core.so", comps)["name"] == "Qt 6"
    assert checker.match_component("libcurl.so", comps) is None


@pytest.mark.parametrize("platforms, current, applies", [
    (["linux"], "linux", True),
    (["linux"], "macos", False),
    (["macos"], "macos", True),
    (["macos"], "linux", False),
    (None, "linux", True),
    (None, "macos", True),
    # No platform named: only an unscoped entry applies. Omitting it used to
    # wave everything through, so a Linux entry covered a macOS bundle.
    (["linux"], None, False),
    (None, None, True),
])
def test_platform_scope(platforms, current, applies):
    c = _comp("libx.so", platforms=platforms)
    assert (checker.match_component("libx.so", [c], current) is c) is applies


def test_internal_globs_cover_both_extensions():
    manifest = json.loads(
        (pathlib.Path(__file__).resolve().parents[2] / "licenses" / "manifest.json")
        .read_text()
    )
    internal = manifest["internal_sonames"]
    for name in ("librescrs-pkcs11.dylib", "libLibreSCRS_SmartCard.dylib",
                 "libid-card-plugin.dylib", "piv-gui-plugin.dylib",
                 "piv-gui-plugin.so", "libresign-core.dylib"):
        assert checker.is_internal(name, internal), name
    assert not checker.is_internal("libcurl.so", internal)


def test_emit_candidates_lists_only_the_unmapped(tmp_path):
    for n in ("libcurl.so.4", "libQt6Core.so.6", "libLibreSCRS_Core.so",
              "libpcsclite.so.1", "libjpeg.so.8"):
        (tmp_path / n).write_bytes(b"")
    m = _man(tmp_path)
    m["components"] = [c for c in m["components"] if c["match"] != "libcurl.so"]
    out = checker.emit_candidates(str(tmp_path), m, "linux")
    assert [c["match"] for c in out] == ["libQt6Core.so", "libcurl.so"]
    assert all(c["name"] == c["spdx"] == c["text"] == c["sha256"] == "" for c in out)
    # A component scoped to the other platform does not shield an asset.
    m["components"].append(_comp("libcurl.so", platforms=["macos"]))
    assert [c["match"] for c in checker.emit_candidates(str(tmp_path), m, "linux")] \
        == ["libQt6Core.so", "libcurl.so"]
    assert [c["match"] for c in checker.emit_candidates(str(tmp_path), m, "macos")] \
        == ["libQt6Core.so"]


# --- the licence walk ------------------------------------------------------


@pytest.mark.parametrize("files, sha, ok", [
    (["libcurl.so.4"], LICENCE_SHA, True),
    (["libcurl.so.4", "libLibreSCRS_Core.so", "libpcsclite.so.1"], LICENCE_SHA, True),
    (["libmystery.so.1"], LICENCE_SHA, False),
    (["libcurl.so.4"], "deadbeef", False),
    # The vacuum: a walk over nothing once reported "0 errors" and rc 0.
    ([], LICENCE_SHA, False),
])
def test_check(tmp_path, files, sha, ok):
    root = _staged(tmp_path, files)
    rc = checker.run_check(root, _man(tmp_path, sha), str(tmp_path), "linux")
    assert (rc == 0) is ok


def _write_manifest(tmp_path, manifest):
    p = tmp_path / "licenses" / "manifest.json"
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(manifest))
    return str(p)


def test_check_refuses_without_a_platform_before_walking(tmp_path, capsys):
    root = _staged(tmp_path, ["libunmapped.so.1"])
    man = _write_manifest(tmp_path, {"internal_sonames": [], "components": []})
    # Control: with a platform the walk runs and fails on the library.
    assert checker.main(["--check", root, "--manifest", man, "--platform", "linux"]) != 0
    assert "::error::" in capsys.readouterr().out
    with pytest.raises(SystemExit) as exc:
        checker.main(["--check", root, "--manifest", man])
    assert exc.value.code != 0
    out = capsys.readouterr().out
    assert "::error::" not in out and "checked " not in out


def test_two_modes_in_one_invocation_are_refused(tmp_path, capsys):
    root = _staged(tmp_path, ["libcurl.so.4"])
    man = _write_manifest(tmp_path, _man(tmp_path))
    out = tmp_path / "never.cdx.json"
    with pytest.raises(SystemExit) as exc:
        checker.main(["--check", root, "--sbom", root, "--sbom-out", str(out),
                      "--manifest", man, "--platform", "linux",
                      "--app-version", "5.0.0", "--app-name", "LibreCelik"])
    assert exc.value.code == 2
    assert not out.exists()
    assert "separate modes" in capsys.readouterr().err


# --- the notices file ------------------------------------------------------


def test_render_dedupes_sorts_and_is_deterministic(tmp_path):
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True)
    (lic / "mit.txt").write_text("MIT LICENSE BODY")
    (lic / "zlib.txt").write_text("ZLIB LICENSE BODY")
    comps = [
        {"name": "Zebra", "spdx": "MIT", "text": "resources/licenses/mit.txt"},
        {"name": "apple", "spdx": "MIT", "text": "resources/licenses/mit.txt"},
        {"name": "zlib", "spdx": "Zlib", "text": "resources/licenses/zlib.txt"},
    ]
    out = gen.render(comps, {"lc": str(tmp_path)})
    assert out == gen.render(comps, {"lc": str(tmp_path)})
    assert out.count("MIT LICENSE BODY") == 1 and "ZLIB LICENSE BODY" in out
    assert out.index("apple") < out.index("Zebra")


def test_notices_main_filters_by_platform(tmp_path):
    lic = tmp_path / "resources" / "licenses"
    lic.mkdir(parents=True)
    (lic / "mit.txt").write_text("MIT")
    man = _write_manifest(tmp_path, {"components": [
        {"name": "libcross", "spdx": "MIT", "text": "resources/licenses/mit.txt"},
        {"name": "libavahi-client", "spdx": "MIT",
         "text": "resources/licenses/mit.txt", "platforms": ["linux"]},
        {"name": "libSecurityFoundation", "spdx": "MIT",
         "text": "resources/licenses/mit.txt", "platforms": ["macos"]},
    ]})
    for plat, present, absent in (("macos", "libSecurityFoundation", "libavahi-client"),
                                  ("linux", "libavahi-client", "libSecurityFoundation")):
        out = tmp_path / f"notices-{plat}.txt"
        assert gen.main(["--manifest", man, "-o", str(out), "--platform", plat]) == 0
        text = out.read_text()
        assert "libcross" in text and present in text and absent not in text


# --- the bill of materials -------------------------------------------------


def _sbom(tmp_path, root, manifest=None, platform="linux", pins=None):
    return checker.emit_sbom(root, manifest or _man(tmp_path), platform,
                             "LibreCelik", "5.0.0", pins=pins)


def test_sbom_lists_what_the_tree_holds(tmp_path):
    root = _staged(tmp_path, ["libcurl.so.4.8.0", "libjpeg.so.8", "libselinux.so.1",
                              "libpcsclite.so.1"])
    doc, rc = _sbom(tmp_path, root)
    assert rc == 0
    by = {c["name"]: c for c in doc["components"]}
    assert sorted(by) == ["curl", "libjpeg-turbo", "libselinux"]  # excluded absent
    assert doc["metadata"]["component"]["version"] == "5.0.0"
    assert by["curl"]["version"] == "4.8.0"
    assert by["curl"]["hashes"] == [
        {"alg": "SHA-256", "content": hashlib.sha256(b"x").hexdigest()}]
    assert by["curl"]["licenses"][0] == {"license": {"id": "curl"}}
    assert by["libjpeg-turbo"]["licenses"][0] == {
        "expression": "IJG AND BSD-3-Clause AND Zlib"}
    assert by["libselinux"]["licenses"][0] == {
        "license": {"name": "LicenseRef-libselinux-public-domain"}}


def test_sbom_refusals_are_told_apart(tmp_path, capsys):
    # An empty TREE (the path is wrong) is rc 1 with its own message; a tree
    # whose objects are all deliberately-not-bundled would publish an empty
    # bill -- the shape that once shipped, signed -- and is rc 2.
    (tmp_path / "empty").mkdir()
    doc, rc = _sbom(tmp_path, str(tmp_path / "empty"))
    assert doc is None and rc == 1
    assert "no shared objects found under" in capsys.readouterr().out
    doc, rc = _sbom(tmp_path, _staged(tmp_path, ["libpcsclite.so.1"]))
    assert doc is None and rc == 2
    doc, rc = _sbom(tmp_path, _staged(tmp_path, ["libmystery.so.1"]))
    assert doc is None and rc == 1


def test_sbom_names_our_own_objects_by_soname_without_a_licence(tmp_path):
    m = _man(tmp_path)
    m["internal_sonames"] = ["libLibreSCRS_*.so", "librescrs-pkcs11.so", "libresign*.so"]
    root = _staged(tmp_path, ["libcurl.so.4", "librescrs-pkcs11.so",
                              "libresign.so.5.0.0", "libLibreSCRS_Core.so.5.0.0"])
    doc, rc = _sbom(tmp_path, root, m)
    assert rc == 0
    by = {c["name"]: c for c in doc["components"]}
    # The ecosystem "lib" strip applied to our names would bill "rescrs-pkcs11".
    assert by["librescrs-pkcs11"]["purl"] == "pkg:generic/librescrs-pkcs11@5.0.0"
    assert by["libresign"]["purl"] == "pkg:generic/libresign@5.0.0"
    assert by["curl"]["purl"] == "pkg:generic/curl@4"
    internal = [c for c in doc["components"]
                if {"name": "librescrs:internal", "value": "true"} in c["properties"]]
    assert internal and all("licenses" not in c for c in internal)


def test_sbom_framework_version_and_hash_come_from_the_bundle(tmp_path):
    fw = tmp_path / "app" / "Contents" / "Frameworks" / "QtCore.framework"
    (fw / "Resources").mkdir(parents=True)
    (fw / "QtCore").write_bytes(b"x")
    with open(fw / "Resources" / "Info.plist", "wb") as fh:
        plistlib.dump({"CFBundleShortVersionString": "6.10.0"}, fh)
    m = _man(tmp_path)
    m["components"].append({"match": "QtCore.framework", "name": "QtCore",
                            "spdx": "LGPL-3.0-or-later",
                            "text": "resources/licenses/curl.txt",
                            "sha256": LICENCE_SHA, "platforms": ["macos"]})
    doc, rc = _sbom(tmp_path, str(tmp_path / "app"), m, "macos")
    assert rc == 0
    qt = [c for c in doc["components"] if c["name"] == "QtCore"][0]
    assert qt["version"] == "6.10.0"
    assert qt["hashes"][0]["content"] == hashlib.sha256(b"x").hexdigest()


def test_sbom_paths_stay_inside_a_symlinked_root(tmp_path):
    # macOS mktemp hands back /var/..., a symlink to /private/var: an
    # unresolved root put the build host's temp path into a signed bill.
    real = tmp_path / "realroot" / "usr" / "lib"
    real.mkdir(parents=True)
    (real / "libcurl.so.4").write_bytes(b"x")
    (tmp_path / "link").symlink_to(tmp_path / "realroot")
    doc, rc = _sbom(tmp_path, str(tmp_path / "link"))
    assert rc == 0
    (comp,) = doc["components"]
    assert comp["bom-ref"] == "usr/lib/libcurl.so.4"


def test_sbom_main_takes_the_app_name_it_is_given(tmp_path):
    root = _staged(tmp_path, ["libcurl.so.4"])
    man = _write_manifest(tmp_path, _man(tmp_path))
    out = tmp_path / "bill.cdx.json"
    assert checker.main(["--sbom", root, "--sbom-out", str(out), "--manifest", man,
                         "--platform", "linux", "--app-version", "5.0.0",
                         "--app-name", "LibreCelik"]) == 0
    assert json.loads(out.read_text())["metadata"]["component"]["name"] == "LibreCelik"


# --- --pins: the commit a network-fetched source was built from ------------


def _pins(tmp_path, body):
    p = tmp_path / "pins.txt"
    p.write_text("# comment\n\n" + body)
    return checker.load_pins(str(p))


JASPER_ROW = f"jasper  https://github.com/jasper-software/jasper.git  {JASPER_SHA}  version-4.2.9\n"


def test_pins_put_the_commit_and_url_in_the_bill(tmp_path):
    root = _staged(tmp_path, ["libjasper.so.7", "libcurl.so.4"])
    doc, rc = _sbom(tmp_path, root, pins=_pins(tmp_path, JASPER_ROW))
    assert rc == 0
    by = {c["name"]: c for c in doc["components"]}
    props = {p["name"]: p["value"] for p in by["JasPer"]["properties"]}
    assert by["JasPer"]["version"] == "version-4.2.9"
    assert props["librescrs:source-commit"] == JASPER_SHA
    assert props["librescrs:source-url"] == "https://github.com/jasper-software/jasper.git"
    assert by["curl"]["version"] == "4"
    # Without a pins file the soname version stands.
    doc, rc = _sbom(tmp_path, _staged(tmp_path, ["libjasper.so.7"]))
    assert rc == 0 and [c for c in doc["components"] if c["name"] == "JasPer"][0]["version"] == "7"


@pytest.mark.parametrize("bundled, row", [
    # a pinned component the file does not name
    (["libjasper.so.7"],
     "qtimageformats  https://code.qt.io/qt/qtimageformats.git  "
     "cc5f5661ef75f08da7064227de38bab6cc3b857c  6.10.3\n"),
    # a pin that matched nothing bundled
    (["libcurl.so.4"], JASPER_ROW),
])
def test_pins_refuse_a_mismatch(tmp_path, capsys, bundled, row):
    doc, rc = _sbom(tmp_path, _staged(tmp_path, bundled), pins=_pins(tmp_path, row))
    assert doc is None and rc == 1
    assert "'jasper'" in capsys.readouterr().out


def test_pins_file_refuses_a_tag_for_a_commit(tmp_path):
    with pytest.raises(ValueError, match="40"):
        _pins(tmp_path, "jasper  https://github.com/jasper-software/jasper.git  "
                        "version-4.2.9  version-4.2.9\n")
