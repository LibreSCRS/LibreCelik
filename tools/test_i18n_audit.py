#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""Self-tests for tools/i18n_audit.py — required green before the tool ships.

Layout: one class per audit dimension (D1, D2, D3, D5, D6, D8) plus
parser/allowlist/determinism cases. Synthetic fixtures live inline as
triple-quoted strings; tmp_path materialises them per test.

See tools/README-i18n-audit.md for the audit design.
"""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Iterable

import pytest

THIS_DIR = Path(__file__).resolve().parent
sys.path.insert(0, str(THIS_DIR))

import i18n_audit as ia  # noqa: E402  (path-sensitive import)


# --------------------------------------------------------------------------
# Fixture helpers
# --------------------------------------------------------------------------

EMPTY_TS = """<?xml version="1.0" encoding="utf-8"?>
<!DOCTYPE TS>
<TS version="2.1" language="{lang}">
<context>
    <name></name>
{messages}
</context>
</TS>
"""


def _make_ts(path: Path, lang: str, ids: Iterable[str]) -> None:
    msgs = "\n".join(
        f'    <message id="{i}">\n        <source>{i}</source>\n'
        f"        <translation>{i}</translation>\n    </message>"
        for i in ids
    )
    path.write_text(EMPTY_TS.format(lang=lang, messages=msgs), encoding="utf-8")


def _make_catalog(path: Path, ids: Iterable[str]) -> None:
    body_lines = [f'    QT_TRID_NOOP("{i}");' for i in ids]
    path.write_text(
        "// SPDX-License-Identifier: GPL-3.0-or-later\n"
        "static void _translationsCatalog() {\n"
        + "\n".join(body_lines)
        + "\n}\n",
        encoding="utf-8",
    )


def _materialise(tmp_path: Path, files: dict[str, str], ids_in_catalog: Iterable[str] | None = None) -> Path:
    """Create a synthetic LC-like tree under tmp_path. Returns the repo
    root. `files` maps repo-relative paths to source contents. If
    `ids_in_catalog` is None, the catalog (and .ts) gets every id found
    in the source files (so D5/D6 are quiescent unless tested
    separately)."""

    repo = tmp_path / "lc"
    (repo / "src").mkdir(parents=True, exist_ok=True)
    (repo / "src" / "utils").mkdir(parents=True, exist_ok=True)
    (repo / "plugins").mkdir(parents=True, exist_ok=True)
    (repo / "tools").mkdir(parents=True, exist_ok=True)
    (repo / "resources" / "i18n").mkdir(parents=True, exist_ok=True)

    for relpath, content in files.items():
        p = repo / relpath
        p.parent.mkdir(parents=True, exist_ok=True)
        p.write_text(content, encoding="utf-8")

    if ids_in_catalog is None:
        # auto-populate from sources.
        ids: set[str] = set()
        for content in files.values():
            stripped = ia.strip_comments_and_strings(content)
            for m in ia._QTTRID_ANY_RE.finditer(stripped):
                if stripped[m.start() : m.start() + len("qtTrId(")] != "qtTrId(":
                    continue
                lm = ia._QTTRID_LITERAL_RE.match(content, m.start())
                if lm is not None:
                    ids.add(lm.group(1))
        ids_list = sorted(ids)
    else:
        ids_list = list(ids_in_catalog)

    _make_ts(repo / "resources" / "i18n" / "LibreCelik_en.ts", "en_US", ids_list)
    _make_ts(repo / "resources" / "i18n" / "LibreCelik_sr_RS.ts", "sr_RS", ids_list)
    _make_catalog(repo / "src" / "utils" / "translations_catalog.cpp", ids_list)
    return repo


def _run(repo: Path, *, dims: list[str] | None = None, allowlist: Path | None = None, strict: bool = False) -> tuple[int, list[ia.Finding]]:
    argv = ["--repo-root", str(repo), "--src-root", str(repo / "src"), "--src-root", str(repo / "plugins")]
    if dims is not None:
        argv += ["--dims", ",".join(dims)]
    if allowlist is not None:
        argv += ["--allowlist", str(allowlist)]
    if strict:
        argv.append("--strict")
    json_path = repo / "out.json"
    argv += ["--json", str(json_path)]
    rc = ia.main(argv)
    if json_path.is_file():
        data = json.loads(json_path.read_text(encoding="utf-8"))
        findings = [
            ia.Finding(
                file=f["file"],
                line=f["line"],
                dim=f["dim"],
                severity=f["severity"],
                cls=f["class"],
                message=f["message"],
                fix_hint=f["fix_hint"],
                allowlisted=f["allowlisted"],
            )
            for f in data["findings"]
        ]
    else:
        findings = []
    return rc, findings


# --------------------------------------------------------------------------
# D1 — handler coverage
# --------------------------------------------------------------------------


class TestD1:
    def test_class_with_qttrid_no_handler_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.h": (
                "#pragma once\nclass BarWidget {\npublic:\n  BarWidget();\n};\n"
            ),
            "src/foo/bar.cpp": (
                '#include "bar.h"\n'
                'BarWidget::BarWidget() { auto x = qtTrId("lc-bar-x"); }\n'
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D1"])
        assert rc == 1
        assert len(fs) == 1
        assert fs[0].dim == "D1"
        assert fs[0].cls == "BarWidget"

    def test_class_with_change_event_language_change_is_clean(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                'BarWidget::BarWidget() { auto x = qtTrId("lc-bar-x"); }\n'
                "void BarWidget::changeEvent(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) retranslateUi();\n"
                "}\n"
                'void BarWidget::retranslateUi() { auto x = qtTrId("lc-bar-x"); }\n'
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D1"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_class_with_event_override_handles_language_change(self, tmp_path):
        # mimics WizardHeaderWidget
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                'BarWidget::BarWidget() { auto x = qtTrId("lc-bar-x"); }\n'
                "bool BarWidget::event(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) rebuild();\n"
                "  return QWidget::event(e);\n"
                "}\n"
                'void BarWidget::rebuild() { auto x = qtTrId("lc-bar-x"); }\n'
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D1"])
        assert rc == 0


# --------------------------------------------------------------------------
# D2 — handler completeness (transitive closure)
# --------------------------------------------------------------------------


class TestD2:
    def test_method_outside_closure_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                "void BarWidget::changeEvent(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) retranslateUi();\n"
                "}\n"
                'void BarWidget::retranslateUi() { auto x = qtTrId("lc-r"); }\n'
                'void BarWidget::buildExtra() { auto x = qtTrId("lc-extra"); }\n'
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D2"])
        assert rc == 1
        assert any("buildExtra" in f.message for f in fs)

    def test_helper_called_from_retranslate_is_clean(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                'void BarWidget::retranslateUi() { applyTitle(); applyBody(); }\n'
                'void BarWidget::applyTitle() { auto x = qtTrId("lc-title"); }\n'
                'void BarWidget::applyBody() { auto x = qtTrId("lc-body"); }\n'
                "void BarWidget::changeEvent(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) retranslateUi();\n"
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D2"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_inline_allowlist_silences_d2(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                "void BarWidget::changeEvent(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) retranslateUi();\n"
                "}\n"
                'void BarWidget::retranslateUi() { auto x = qtTrId("lc-r"); }\n'
                'void BarWidget::buildExtra() { auto x = qtTrId("lc-extra");  // i18n-audit: ignore D2, parent rebuilds widget on language change\n'
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D2"])
        assert rc == 0, [f.__dict__ for f in fs]


# --------------------------------------------------------------------------
# D3 — static qtTrId
# --------------------------------------------------------------------------


class TestD3:
    def test_inline_static_pattern_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.cpp": (
                "QString helper() {\n"
                '  static QString s = qtTrId("lc-helper");\n'
                "  return s;\n"
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D3"])
        assert rc == 1
        assert any(f.dim == "D3" for f in fs)

    def test_decl_then_assign_pattern_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.cpp": (
                "void seed() {\n"
                "  static QString s;\n"
                '  s = qtTrId("lc-seed");\n'
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D3"])
        assert rc == 1

    def test_inline_allowlist_silences_d3(self, tmp_path):
        files = {
            "src/foo/bar.cpp": (
                "QString helper() {\n"
                '  static const QString kHelpUrl = qtTrId("lc-help-url");  // i18n-audit: ignore D3, locale-stable URL\n'
                "  return kHelpUrl;\n"
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D3"])
        assert rc == 0, [f.__dict__ for f in fs]


# --------------------------------------------------------------------------
# D5 — catalog triple consistency
# --------------------------------------------------------------------------


class TestD5:
    def test_missing_from_catalog_noop_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.cpp": 'void f() { auto x = qtTrId("lc-new-id"); }\n',
        }
        # Only one of the three sinks gets the new id; D5 must report.
        repo = _materialise(tmp_path, files, ids_in_catalog=[])
        rc, fs = _run(repo, dims=["D5"])
        assert rc == 1
        assert any(f.dim == "D5" and "lc-new-id" in f.message for f in fs)

    def test_present_in_all_three_is_clean(self, tmp_path):
        files = {
            "src/foo/bar.cpp": 'void f() { auto x = qtTrId("lc-ok"); }\n',
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D5"])
        assert rc == 0


# --------------------------------------------------------------------------
# D6 — orphan catalog IDs
# --------------------------------------------------------------------------


class TestD6:
    def test_orphan_catalog_id_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.cpp": 'void f() { auto x = qtTrId("lc-used"); }\n',
        }
        repo = _materialise(tmp_path, files, ids_in_catalog=["lc-used", "lc-orphan"])
        rc, fs = _run(repo, dims=["D6"])
        assert rc == 1
        assert any("lc-orphan" in f.message for f in fs)

    def test_dynamic_callsite_suppresses_orphan(self, tmp_path):
        # qtTrId(KEY_CONST) → unresolved → conservative suppression of
        # any "lc-*" orphan.
        files = {
            "src/foo/bar.cpp": (
                "void f() {\n"
                "  const char* KEY_CONST = \"lc-foo-a\";\n"
                "  auto x = qtTrId(KEY_CONST);\n"
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files, ids_in_catalog=["lc-foo-a", "lc-foo-b"])
        rc, fs = _run(repo, dims=["D6"])
        assert rc == 0, [f.__dict__ for f in fs]


# --------------------------------------------------------------------------
# D8 — tooltip / accessibleName / setStatusTip / setWhatsThis
# --------------------------------------------------------------------------


class TestD8:
    def test_tooltip_outside_retranslate_is_flagged(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                "void BarWidget::changeEvent(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) retranslateUi();\n"
                "}\n"
                "void BarWidget::retranslateUi() {}\n"
                "void BarWidget::buildButton() {\n"
                '  btn->setToolTip(qtTrId("lc-tip"));\n'
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D8"])
        assert rc == 1
        assert any(f.dim == "D8" for f in fs)

    def test_tooltip_inside_retranslate_is_clean(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass BarWidget {};\n",
            "src/foo/bar.cpp": (
                "void BarWidget::changeEvent(QEvent* e) {\n"
                "  if (e->type() == QEvent::LanguageChange) retranslateUi();\n"
                "}\n"
                "void BarWidget::retranslateUi() {\n"
                '  btn->setToolTip(qtTrId("lc-tip"));\n'
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D8"])
        assert rc == 0


# --------------------------------------------------------------------------
# D9 — numerus discipline (plural-aware declarations + .ts plural forms)
# --------------------------------------------------------------------------


class TestD9:
    def _repo(
        self,
        tmp_path,
        catalog_body: str,
        en_messages: str = "",
        sr_messages: str = "",
    ) -> Path:
        repo = _materialise(tmp_path, {})
        (repo / "src" / "utils" / "translations_catalog.cpp").write_text(
            "// SPDX-License-Identifier: GPL-3.0-or-later\n"
            "static void _translationsCatalog() {\n" + catalog_body + "}\n",
            encoding="utf-8",
        )
        (repo / "resources" / "i18n" / "LibreCelik_en.ts").write_text(
            EMPTY_TS.format(lang="en_US", messages=en_messages), encoding="utf-8"
        )
        (repo / "resources" / "i18n" / "LibreCelik_sr_RS.ts").write_text(
            EMPTY_TS.format(lang="sr_RS", messages=sr_messages), encoding="utf-8"
        )
        return repo

    def test_percent_n_source_with_singular_noop_is_flagged(self, tmp_path):
        # The exact defect that shipped: lupdate reads the singular NOOP and
        # strips ("Removed plural forms") the hand-maintained numerus entries.
        repo = self._repo(
            tmp_path,
            '    //% "Each file is confirmed separately — %n confirmations."\n'
            '    QT_TRID_NOOP("lc-seq");\n',
        )
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 1
        assert any(f.dim == "D9" and "lc-seq" in f.message for f in fs)

    def test_percent_n_source_with_plural_noop_is_clean(self, tmp_path):
        en = (
            '    <message numerus="yes" id="lc-seq">\n'
            "        <source>— %n confirmations.</source>\n"
            "        <translation>\n"
            "            <numerusform>— %n confirmation.</numerusform>\n"
            "            <numerusform>— %n confirmations.</numerusform>\n"
            "        </translation>\n"
            "    </message>"
        )
        sr = (
            '    <message numerus="yes" id="lc-seq">\n'
            "        <source>— %n confirmations.</source>\n"
            "        <translation>\n"
            "            <numerusform>— %n потврда.</numerusform>\n"
            "            <numerusform>— %n потврде.</numerusform>\n"
            "            <numerusform>— %n потврда.</numerusform>\n"
            "        </translation>\n"
            "    </message>"
        )
        repo = self._repo(
            tmp_path,
            '    //% "— %n confirmations."\n    QT_TRID_N_NOOP("lc-seq");\n',
            en_messages=en,
            sr_messages=sr,
        )
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_plural_noop_id_counts_as_catalog_entry_for_d5(self, tmp_path):
        # Regression guard: switching a declaration NOOP → N_NOOP must not
        # make D5 report the id as missing from translations_catalog.cpp.
        files = {"src/foo/bar.cpp": 'void f() { auto x = qtTrId("lc-seq"); }\n'}
        repo = _materialise(tmp_path, files, ids_in_catalog=["lc-seq"])
        (repo / "src" / "utils" / "translations_catalog.cpp").write_text(
            "static void _translationsCatalog() {\n"
            '    //% "%n things"\n'
            '    QT_TRID_N_NOOP("lc-seq");\n'
            "}\n",
            encoding="utf-8",
        )
        rc, fs = _run(repo, dims=["D5"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_ts_percent_n_without_numerus_is_flagged(self, tmp_path):
        # The post-destruction state: source still says %n but the message
        # lost its numerus="yes" (what the destructive lupdate leaves behind).
        en = (
            '    <message id="lc-seq">\n'
            "        <source>— %n confirmations.</source>\n"
            '        <translation type="unfinished"></translation>\n'
            "    </message>"
        )
        repo = self._repo(
            tmp_path,
            '    //% "— %n confirmations."\n    QT_TRID_N_NOOP("lc-seq");\n',
            en_messages=en,
        )
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 1
        assert any(f.dim == "D9" and "numerus" in f.message for f in fs)

    def test_sr_numerus_with_too_few_forms_is_flagged(self, tmp_path):
        # Serbian needs three plural forms; a collapse to one is a loss even
        # when numerus="yes" survives.
        sr = (
            '    <message numerus="yes" id="lc-seq">\n'
            "        <source>— %n confirmations.</source>\n"
            "        <translation>\n"
            "            <numerusform>— %n потврда.</numerusform>\n"
            "        </translation>\n"
            "    </message>"
        )
        repo = self._repo(
            tmp_path,
            '    //% "— %n confirmations."\n    QT_TRID_N_NOOP("lc-seq");\n',
            sr_messages=sr,
        )
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 1
        assert any(f.dim == "D9" and "numerusform" in f.message for f in fs)

    def test_sr_latin_numerus_with_too_few_forms_is_flagged(self, tmp_path):
        # D9 must also cover the script-derived Serbian Latin catalogue
        # (LibreCelik_sr_Latn_RS.ts), not just the canonical en/sr pair —
        # it is transliterated by a script rather than produced by lupdate,
        # so it gets no other numerus-discipline check of its own.
        repo = self._repo(
            tmp_path,
            '    //% "— %n confirmations."\n    QT_TRID_N_NOOP("lc-seq");\n',
        )
        latin = (
            '    <message numerus="yes" id="lc-seq">\n'
            "        <source>— %n confirmations.</source>\n"
            "        <translation>\n"
            "            <numerusform>— %n potvrda.</numerusform>\n"
            "        </translation>\n"
            "    </message>"
        )
        (repo / "resources" / "i18n" / "LibreCelik_sr_Latn_RS.ts").write_text(
            EMPTY_TS.format(lang="sr_Latn_RS", messages=latin), encoding="utf-8"
        )
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 1
        assert any(
            f.dim == "D9" and "numerusform" in f.message and "sr_Latn_RS" in f.file for f in fs
        )

    def test_sr_latin_ts_absent_is_not_flagged(self, tmp_path):
        # No LibreCelik_sr_Latn_RS.ts in the tree (e.g. a repo state before
        # this catalogue existed) must not be treated as a D9 finding —
        # detect_d9 skips files that are not present.
        repo = self._repo(
            tmp_path,
            '    //% "— %n confirmations."\n    QT_TRID_N_NOOP("lc-seq");\n',
        )
        assert not (repo / "resources" / "i18n" / "LibreCelik_sr_Latn_RS.ts").exists()
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_malformed_ts_is_flagged_not_silently_skipped(self, tmp_path):
        # A broken catalogue must fail the gate, not read as clean because
        # ET.parse() raised and the old code swallowed the exception.
        repo = self._repo(
            tmp_path,
            '    //% "— %n confirmations."\n    QT_TRID_N_NOOP("lc-seq");\n',
        )
        (repo / "resources" / "i18n" / "LibreCelik_sr_RS.ts").write_text(
            '<?xml version="1.0" encoding="utf-8"?>\n<TS version="2.1" language="sr_RS"><unclosed',
            encoding="utf-8",
        )
        rc, fs = _run(repo, dims=["D9"])
        assert rc == 1
        assert any(f.dim == "D9" and "malformed XML" in f.message for f in fs)


# --------------------------------------------------------------------------
# D10 — derived-catalogue parity (msgid set + unfinished count vs source)
# --------------------------------------------------------------------------


class TestD10:
    def _repo_with_source(self, tmp_path, ids: list[str]) -> Path:
        # ids_in_catalog fixes the exact msgid set in en/sr/catalog.cpp so
        # the derived-catalogue comparison target is known precisely.
        return _materialise(tmp_path, {}, ids_in_catalog=ids)

    def _write_derived(self, repo: Path, name: str, lang: str, messages: str) -> Path:
        p = repo / "resources" / "i18n" / name
        p.write_text(EMPTY_TS.format(lang=lang, messages=messages), encoding="utf-8")
        return p

    def test_derived_catalog_missing_msgid_is_flagged(self, tmp_path):
        repo = self._repo_with_source(tmp_path, ["lc-a", "lc-b"])
        self._write_derived(
            repo,
            "LibreCelik_sr_Latn_RS.ts",
            "sr_Latn_RS",
            '    <message id="lc-a">\n        <source>lc-a</source>\n'
            "        <translation>lc-a</translation>\n    </message>",
        )
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 1
        assert any(f.dim == "D10" and "missing" in f.message and "lc-b" in f.message for f in fs)

    def test_derived_catalog_extra_msgid_is_flagged(self, tmp_path):
        repo = self._repo_with_source(tmp_path, ["lc-a"])
        self._write_derived(
            repo,
            "LibreCelik_sr_Latn_RS.ts",
            "sr_Latn_RS",
            '    <message id="lc-a">\n        <source>lc-a</source>\n'
            "        <translation>lc-a</translation>\n    </message>\n"
            '    <message id="lc-ghost">\n        <source>lc-ghost</source>\n'
            "        <translation>lc-ghost</translation>\n    </message>",
        )
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 1
        assert any(
            f.dim == "D10" and "present only in" in f.message and "lc-ghost" in f.message for f in fs
        )

    def test_derived_catalog_matching_msgids_is_clean(self, tmp_path):
        repo = self._repo_with_source(tmp_path, ["lc-a", "lc-b"])
        self._write_derived(
            repo,
            "LibreCelik_sr_Latn_RS.ts",
            "sr_Latn_RS",
            '    <message id="lc-a">\n        <source>lc-a</source>\n'
            "        <translation>lc-a</translation>\n    </message>\n"
            '    <message id="lc-b">\n        <source>lc-b</source>\n'
            "        <translation>lc-b</translation>\n    </message>",
        )
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_derived_catalog_extra_unfinished_is_flagged(self, tmp_path):
        # _make_ts's default sr_RS entry for "lc-a" is finished; the derived
        # catalogue regresses it to unfinished — that must be a finding
        # even though the msgid sets match.
        repo = self._repo_with_source(tmp_path, ["lc-a"])
        self._write_derived(
            repo,
            "LibreCelik_sr_Latn_RS.ts",
            "sr_Latn_RS",
            '    <message id="lc-a">\n        <source>lc-a</source>\n'
            '        <translation type="unfinished"></translation>\n    </message>',
        )
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 1
        assert any(f.dim == "D10" and "unfinished" in f.message and "lc-a" in f.message for f in fs)

    def test_missing_derived_catalog_is_not_flagged(self, tmp_path):
        repo = self._repo_with_source(tmp_path, ["lc-a"])
        assert not (repo / "resources" / "i18n" / "LibreCelik_sr_Latn_RS.ts").exists()
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 0, [f.__dict__ for f in fs]

    def test_malformed_source_ts_is_flagged(self, tmp_path):
        repo = self._repo_with_source(tmp_path, ["lc-a"])
        (repo / "resources" / "i18n" / "LibreCelik_sr_RS.ts").write_text(
            '<?xml version="1.0" encoding="utf-8"?>\n<TS version="2.1" language="sr_RS"><unclosed',
            encoding="utf-8",
        )
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 1
        assert any(f.dim == "D10" and "malformed XML" in f.message for f in fs)


# --------------------------------------------------------------------------
# Catalog discovery — self-maintaining glob, not a hardcoded path
# --------------------------------------------------------------------------


class TestDerivedCatalogDiscovery:
    def test_glob_discovers_any_additional_catalog_without_hardcoding(self, tmp_path):
        # Proves discover_ts_catalogs() is a glob over
        # resources/i18n/LibreCelik_*.ts, not a name the tool has to know
        # in advance: a hypothetical future derived catalogue under a name
        # this tool has never seen is still picked up by D9/D10.
        repo = _materialise(tmp_path, {}, ids_in_catalog=["lc-a"])
        weird = repo / "resources" / "i18n" / "LibreCelik_sr_Ijek_RS.ts"
        weird.write_text(
            EMPTY_TS.format(
                lang="sr_Ijek_RS",
                messages=(
                    '    <message id="lc-a">\n        <source>lc-a</source>\n'
                    '        <translation type="unfinished"></translation>\n    </message>'
                ),
            ),
            encoding="utf-8",
        )
        rc, fs = _run(repo, dims=["D10"])
        assert rc == 1
        assert any(f.dim == "D10" and "LibreCelik_sr_Ijek_RS.ts" in f.file for f in fs)

    def test_canonical_catalogs_are_not_treated_as_derived(self, tmp_path):
        repo = _materialise(tmp_path, {}, ids_in_catalog=["lc-a"])
        _, _, derived = ia.discover_ts_catalogs(repo)
        names = {p.name for p in derived}
        assert "LibreCelik_en.ts" not in names
        assert "LibreCelik_sr_RS.ts" not in names


# --------------------------------------------------------------------------
# Allowlist syntax
# --------------------------------------------------------------------------


class TestAllowlist:
    def test_inline_allowlist_without_reason_aborts_with_exit_2(self, tmp_path):
        # Missing comma+reason is rejected at parse time → exit 2.
        files = {
            "src/foo/bar.cpp": (
                "void f() {\n"
                '  static const QString s = qtTrId("lc-x");  // i18n-audit: ignore D3\n'
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, _ = _run(repo, dims=["D3"])
        assert rc == 2

    def test_file_level_allowlist_silences_finding(self, tmp_path):
        files = {
            "src/foo/bar.cpp": (
                "void f() {\n"
                '  static QString s = qtTrId("lc-x");\n'
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        al = repo / "allowlist"
        al.write_text("src/foo/bar.cpp:D3:legacy oneshot, removed in 4.1\n", encoding="utf-8")
        rc, fs = _run(repo, dims=["D3"], allowlist=al)
        assert rc == 0, [f.__dict__ for f in fs]
        # finding still emitted but marked allowlisted
        assert all(f.allowlisted for f in fs)

    def test_malformed_file_level_line_aborts_with_exit_2(self, tmp_path):
        files = {"src/foo/bar.cpp": "void f() {}\n"}
        repo = _materialise(tmp_path, files)
        al = repo / "allowlist"
        al.write_text("malformed-line-no-colons\n", encoding="utf-8")
        rc, _ = _run(repo, dims=["D1"], allowlist=al)
        assert rc == 2


# --------------------------------------------------------------------------
# Parser / preprocessor
# --------------------------------------------------------------------------


class TestParser:
    def test_qttrid_inside_line_comment_is_ignored(self, tmp_path):
        files = {
            # The literal qtTrId(\"lc-comment\") is inside // and must
            # NOT register as a callsite.
            "src/foo/bar.cpp": (
                "// example: when adding qtTrId(\"lc-comment\") update the catalog\n"
                "void f() { auto x = 1; }\n"
            ),
        }
        repo = _materialise(tmp_path, files, ids_in_catalog=[])
        rc, fs = _run(repo, dims=["D5"])
        # No qtTrId callsite anywhere → D5 should report nothing.
        assert rc == 0, [f.__dict__ for f in fs]

    def test_qttrid_inside_block_comment_is_ignored(self, tmp_path):
        files = {
            "src/foo/bar.cpp": (
                "/* See: qtTrId(\"lc-blocked\") */\n"
                "void f() { auto x = 1; }\n"
            ),
        }
        repo = _materialise(tmp_path, files, ids_in_catalog=[])
        rc, fs = _run(repo, dims=["D5"])
        assert rc == 0

    def test_qttrid_inside_string_literal_is_ignored(self, tmp_path):
        files = {
            "src/foo/bar.cpp": (
                "void f() {\n"
                '  const char* doc = "we use qtTrId(\\"lc-doc\\") here";\n'
                "  (void)doc;\n"
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files, ids_in_catalog=[])
        rc, fs = _run(repo, dims=["D5"])
        assert rc == 0

    def test_method_def_at_column_zero_only(self, tmp_path):
        # std::any_of inside a body with a lambda body must NOT be
        # mistaken for a method definition of class "std".
        files = {
            "src/foo/bar.h": "#pragma once\nclass Foo {};\n",
            "src/foo/bar.cpp": (
                "void Foo::run() {\n"
                "  auto pred = []{};\n"
                "  std::any_of(v.begin(), v.end(), [](int x){ return x > 0; });\n"
                "  qtTrId(\"lc-foo-x\");\n"
                "}\n"
            ),
        }
        repo = _materialise(tmp_path, files)
        rc, fs = _run(repo, dims=["D1"])
        assert all(f.cls != "std" for f in fs)


# --------------------------------------------------------------------------
# Determinism
# --------------------------------------------------------------------------


class TestDeterminism:
    def test_two_runs_produce_byte_identical_json(self, tmp_path):
        files = {
            "src/foo/bar.h": "#pragma once\nclass A {};\nclass B {};\n",
            "src/foo/bar.cpp": (
                'void A::ctor() { qtTrId("lc-a"); }\n'
                'void B::ctor() { qtTrId("lc-b"); }\n'
                'static QString s = qtTrId("lc-s");\n'
            ),
        }
        repo = _materialise(tmp_path, files)
        out1 = repo / "out1.json"
        out2 = repo / "out2.json"
        argv1 = [
            "--repo-root", str(repo),
            "--src-root", str(repo / "src"),
            "--src-root", str(repo / "plugins"),
            "--json", str(out1),
        ]
        argv2 = list(argv1)
        argv2[-1] = str(out2)
        ia.main(argv1)
        ia.main(argv2)
        assert out1.read_bytes() == out2.read_bytes()
