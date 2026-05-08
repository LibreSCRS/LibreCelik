#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2026 hirashix0
"""LibreCelik i18n retranslate audit — static checker.

See knowledge/specs/2026-05-08-i18n-retranslate-audit-design.md (rc2)
for the authoritative design.

Detects six classes of i18n retranslate bug from source + .ts files:

    D1 — class uses qtTrId() but lacks retranslateUi() or
         changeEvent(LanguageChange).
    D2 — retranslateUi() exists but does not transitively cover every
         same-class qtTrId callsite.
    D3 — static QString s = qtTrId(...) snapshots the first-call value.
    D5 — qtTrId ID missing from QT_TRID_NOOP catalog or .ts files.
    D6 — .ts <message id="..."> with no source-file consumer.
    D8 — setToolTip / setAccessibleName / setStatusTip / setWhatsThis
         called with qtTrId outside retranslateUi closure.

Exit codes:

    0 — no findings (after allowlist filtering).
    1 — at least one finding.
    2 — tool error (bad CLI, malformed allowlist, parse failure).

Determinism: identical input → identical output, byte-for-byte.
"""

from __future__ import annotations

import argparse
import json
import re
import subprocess
import sys
import xml.etree.ElementTree as ET
from collections import defaultdict
from dataclasses import asdict, dataclass, field
from pathlib import Path
from typing import IO, Iterable, Iterator, Mapping, Sequence

__version__ = "1.0.0"
SCHEMA_VERSION = 1

# --------------------------------------------------------------------------
# Data model
# --------------------------------------------------------------------------

ALL_DIMS: tuple[str, ...] = ("D1", "D2", "D3", "D5", "D6", "D8")

SEVERITY: Mapping[str, str] = {
    "D1": "high",
    "D2": "high",
    "D3": "high",
    "D5": "medium",
    "D6": "low",
    "D8": "medium",
}

FIX_HINTS: Mapping[str, str] = {
    "D1": (
        "override changeEvent(QEvent*); on QEvent::LanguageChange "
        "call retranslateUi() (see April 2026 retranslation spec)"
    ),
    "D2": (
        "extend retranslateUi() to cover this qtTrId callsite, or "
        "factor it into a same-class helper called from retranslateUi()"
    ),
    "D3": (
        "remove `static`; recompute the qtTrId() result inside the "
        "function (or move the assignment into retranslateUi())"
    ),
    "D5": (
        "add QT_TRID_NOOP(\"<id>\") to src/utils/translations_catalog.cpp "
        "and rerun lupdate so both .ts files contain <message id=\"<id>\">"
    ),
    "D6": (
        "delete the dead <message id=\"...\"> from both .ts files; if the "
        "ID is produced by a dynamic callsite, add an inline allowlist"
    ),
    "D8": (
        "move setToolTip/setAccessibleName/setStatusTip/setWhatsThis "
        "into retranslateUi() (or a same-class helper reachable from it)"
    ),
}


@dataclass(frozen=True, order=True)
class Finding:
    """Single audit finding. Sort key: (file, line, dim) for determinism."""

    file: str
    line: int
    dim: str
    severity: str = field(compare=False)
    cls: str | None = field(compare=False)
    message: str = field(compare=False)
    fix_hint: str = field(compare=False)
    allowlisted: bool = field(default=False, compare=False)


@dataclass(frozen=True)
class AllowlistEntry:
    """Single line from tools/i18n_audit.allowlist."""

    file: str  # repo-relative path, exact match against Finding.file
    dim: str
    reason: str


# --------------------------------------------------------------------------
# Source-tree preprocessing (A.S.2)
# --------------------------------------------------------------------------

# Inline allowlist annotation; reason text is mandatory.
_INLINE_ALLOWLIST_RE = re.compile(r"//\s*i18n-audit:\s*ignore\s+(D\d+),\s*(.+?)\s*$")

_QTTRID_LITERAL_RE = re.compile(r'\bqtTrId\(\s*"([^"\\]*(?:\\.[^"\\]*)*)"\s*[,)]')
_QTTRID_ANY_RE = re.compile(r"\bqtTrId\(\s*([^)]*?)\)")
_QT_TRID_NOOP_RE = re.compile(r'\bQT_TRID_NOOP\(\s*"([^"\\]*(?:\\.[^"\\]*)*)"\s*\)')

# D3 patterns
_D3_INLINE_STATIC_RE = re.compile(
    r"\bstatic\s+(?:const\s+)?(?:[\w:]+\s+)?\w+\s*=\s*qtTrId\b"
)
_D3_DECL_RE = re.compile(r"\bstatic\s+(?:const\s+)?QString\s+(\w+)\s*;")

# Setter calls that must live inside retranslateUi
_SETTER_NAMES = ("setToolTip", "setAccessibleName", "setStatusTip", "setWhatsThis")
_SETTER_QTTRID_RE = re.compile(
    r"\b(" + "|".join(_SETTER_NAMES) + r")\(\s*qtTrId\s*\("
)

# Class declaration: `class FOO_EXPORT? Name : public Base ...` or `struct ...`
_CLASS_DECL_RE = re.compile(
    r"^\s*(?:class|struct)\s+(?:\w+_EXPORT\s+)?(\w+)\b"
    r"(?:\s*final)?(?:\s*:\s*[^;{}]*)?\s*\{",
    re.MULTILINE,
)

# Method definition `RetType ClassName::method(args) ...` (Qt-style
# `void Foo::bar()`). The regex requires the line to START with a
# non-whitespace character (we manually scan after \n) so lambda calls
# / function invocations that appear deep inside other bodies
# (`std::any_of(...)`, `QTimer::singleShot(...)`, indented
# `QMetaObject::invokeMethod(...)`) cannot match — only file-scope
# definitions. The `[\w:<>,\s\*&]+` segment captures the return type
# (allowed to span newlines for templates) and is non-greedy.
_METHOD_DEF_RE = re.compile(
    r"^(?:[\w:<>,\*&][\w:<>,\s\*&]*?)\s+(\w+)::(\w+)\s*\([^;{}]*\)"
    r"(?:\s*const)?(?:\s*noexcept)?(?:\s*override)?(?:\s*final)?\s*\{",
    re.MULTILINE,
)

# Constructor definition `ClassName::ClassName(args) [: init...] {`.
# No return type. Allows an arbitrary member-initialiser list before
# the body brace. Anchored at column 0.
_CTOR_DEF_RE = re.compile(
    r"^(\w+)::(\1)\s*\([^;{}]*\)\s*"
    r"(?::\s*[^;{}]*?)?"
    r"(?:\s*noexcept)?\s*\{",
    re.MULTILINE,
)

# Destructor definition `ClassName::~ClassName() {`.
_DTOR_DEF_RE = re.compile(
    r"^(\w+)::~(\1)\s*\(\s*\)\s*(?:\s*noexcept)?\s*\{",
    re.MULTILINE,
)

# Same-class method call inside a method body: this->name(  or bare name(
_SAME_CLASS_CALL_RE = re.compile(r"(?:\bthis->\s*|\b)(\w+)\s*\(")


@dataclass
class CallSite:
    """qtTrId() or setter callsite."""

    file: str
    line: int
    raw_id: str | None  # the literal id, or None when unresolvable
    is_dynamic: bool


@dataclass
class MethodBody:
    cls: str
    name: str
    file: str
    start_line: int
    end_line: int
    text: str  # already comment/string-stripped (string literals preserved as-is)
    qttrid_callsites: list[CallSite] = field(default_factory=list)
    setter_callsites: list[CallSite] = field(default_factory=list)
    same_class_calls: set[str] = field(default_factory=set)


@dataclass
class ClassRecord:
    name: str
    header: Path | None = None
    impl_files: list[Path] = field(default_factory=list)
    methods: dict[str, MethodBody] = field(default_factory=dict)
    has_retranslate_ui: bool = False
    change_event_branches: set[str] = field(default_factory=set)
    # Free-floating callsites in this class's methods that did not parse as
    # method bodies (e.g. lambdas inside ctor); kept for D1/D2 awareness.
    extra_qttrid_callsites: list[CallSite] = field(default_factory=list)
    extra_setter_callsites: list[CallSite] = field(default_factory=list)
    # Tracks files that declared this class (header or impl); used for
    # finding emission.
    declaring_files: list[str] = field(default_factory=list)


def strip_comments_and_strings(text: str) -> str:
    """Replace // line comments, /* */ block comments, string literals,
    and char literals with same-length neutral placeholders.

    Critical invariant: the returned string has the *same length* and the
    *same line breaks* as the input, so regex offsets are usable as
    indices into either text. Comment/string contents are replaced
    char-by-char with spaces (preserving '\\n'), and string literals are
    replaced with `"` + (spaces) + `"` so the call-shape regex
    `qtTrId\\(\\s*([^)]*?)\\)` still matches but with empty-equivalent
    content. This is essential for offset-faithful detector chaining.
    """

    out: list[str] = list(text)
    i = 0
    n = len(text)
    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""
        # Line comment
        if c == "/" and nxt == "/":
            j = text.find("\n", i)
            if j == -1:
                j = n
            for k in range(i, j):
                out[k] = " "
            i = j  # leave the newline (or end) intact
            continue
        # Block comment
        if c == "/" and nxt == "*":
            j = text.find("*/", i + 2)
            if j == -1:
                j = n
            else:
                j += 2
            for k in range(i, j):
                if text[k] != "\n":
                    out[k] = " "
            i = j
            continue
        # Raw string literal R"delim(...)delim"
        if c == "R" and nxt == '"':
            delim_end = text.find("(", i + 2)
            if delim_end != -1:
                delim = text[i + 2 : delim_end]
                end_marker = ")" + delim + '"'
                j = text.find(end_marker, delim_end + 1)
                if j != -1:
                    end = j + len(end_marker)
                    # Replace the entire raw-string span with spaces, keep newlines.
                    for k in range(i, end):
                        if text[k] != "\n":
                            out[k] = " "
                    i = end
                    continue
            # malformed; fall through
        # String literal "..."
        if c == '"':
            j = i + 1
            while j < n:
                if text[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if text[j] == '"':
                    break
                j += 1
            end = j + 1 if j < n else n
            # Replace the contents (between the quotes) with spaces; keep
            # opening and closing quotes so the call-shape regex still
            # matches the literal-empty form.
            for k in range(i + 1, end - 1):
                if text[k] != "\n":
                    out[k] = " "
            i = end
            continue
        # Char literal '...'
        if c == "'":
            j = i + 1
            while j < n:
                if text[j] == "\\" and j + 1 < n:
                    j += 2
                    continue
                if text[j] == "'":
                    break
                j += 1
            end = j + 1 if j < n else n
            for k in range(i + 1, end - 1):
                if text[k] != "\n":
                    out[k] = " "
            i = end
            continue
        i += 1
    return "".join(out)


def _line_number(text: str, offset: int) -> int:
    """Return 1-based line number of `offset` in `text`."""

    return text.count("\n", 0, offset) + 1


def _find_matching_brace(text: str, open_idx: int) -> int:
    """Return index of the matching '}' for the '{' at open_idx, or -1.

    Operates on already-stripped text so braces inside strings/comments
    are not a concern.
    """

    depth = 0
    n = len(text)
    i = open_idx
    while i < n:
        ch = text[i]
        if ch == "{":
            depth += 1
        elif ch == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    return -1


def _extract_qttrid_callsites(file_str: str, body_text: str, base_line: int) -> tuple[list[CallSite], list[CallSite]]:
    """Return (qttrid_callsites, setter_callsites) inside body_text.

    `body_text` is already comment/string-stripped (string contents
    replaced with empty quotes, but qtTrId(\"lc-foo\") form preserved as
    qtTrId(\"\")). The original literal IDs are recovered via a
    parallel scan on the un-stripped text — that's deferred to the
    higher-level walker that knows the original mapping.

    For now, we walk un-stripped text directly so literal IDs are
    available; the caller passes the original body.
    """
    # Implementation below operates on un-stripped text; this docstring
    # documents the contract but the function is implemented elsewhere.
    raise NotImplementedError  # pragma: no cover  (sentinel; real impl below)


def _scan_callsites(file_str: str, body_text: str, base_line: int) -> tuple[list[CallSite], list[CallSite], set[str]]:
    """Scan a method body (already stripped of // and /* */ comments,
    string contents nulled) for qtTrId callsites, setter callsites, and
    same-class method calls.

    Note: string contents are nulled but `qtTrId("...")` callsites are
    detected via the *un-stripped* text passed in via a parallel offset
    map maintained by the caller. To keep the scanner self-contained,
    we re-run the literal regex on the original (pre-strip) body
    supplied separately.

    Here, `body_text` is the *stripped* body. Literal qtTrId ids cannot
    be recovered from it (the strings are blanked). The caller must
    supply the original body via `_scan_callsites_with_originals`.
    """
    qtt: list[CallSite] = []
    sets: list[CallSite] = []
    calls: set[str] = set()

    for m in _QTTRID_ANY_RE.finditer(body_text):
        line = base_line + body_text.count("\n", 0, m.start())
        qtt.append(CallSite(file=file_str, line=line, raw_id=None, is_dynamic=True))
    for m in _SETTER_QTTRID_RE.finditer(body_text):
        line = base_line + body_text.count("\n", 0, m.start())
        sets.append(CallSite(file=file_str, line=line, raw_id=None, is_dynamic=True))
    for m in _SAME_CLASS_CALL_RE.finditer(body_text):
        name = m.group(1)
        # filter common keywords / non-method calls
        if name in {
            "if", "for", "while", "switch", "return", "sizeof",
            "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",
            "qtTrId", "QString", "qDebug", "qWarning", "qInfo", "qCritical",
            "qFatal", "QT_TRID_NOOP", "Q_EMIT", "emit", "Q_UNUSED",
            "Q_ASSERT", "Q_OBJECT",
        }:
            continue
        calls.add(name)
    return qtt, sets, calls


def _scan_callsites_with_originals(
    file_str: str, original_body: str, stripped_body: str, base_line: int
) -> tuple[list[CallSite], list[CallSite], set[str]]:
    """Authoritative callsite scan.

    `original_body` is the un-stripped C++ source text of the method
    body; `stripped_body` has the same byte length (helper preserves
    structure) so offsets coincide. We anchor every callsite via
    `stripped_body` (so commented-out qtTrId() does not register), then
    recover the literal id via `original_body` at the same offset.
    """

    qtt: list[CallSite] = []
    sets: list[CallSite] = []
    calls: set[str] = set()

    for m in _QTTRID_ANY_RE.finditer(stripped_body):
        # Anchor: stripped text must start with literal "qtTrId(" at this
        # offset. Comments/strings have been replaced; if the offset is
        # inside a stripped span it will not match.
        if stripped_body[m.start() : m.start() + len("qtTrId(")] != "qtTrId(":
            continue
        line = base_line + stripped_body.count("\n", 0, m.start())
        # Recover the literal id from the original text at the same offset.
        lm = _QTTRID_LITERAL_RE.match(original_body, m.start())
        if lm is not None:
            qtt.append(
                CallSite(file=file_str, line=line, raw_id=lm.group(1), is_dynamic=False)
            )
        else:
            qtt.append(CallSite(file=file_str, line=line, raw_id=None, is_dynamic=True))

    for m in _SETTER_QTTRID_RE.finditer(stripped_body):
        line = base_line + stripped_body.count("\n", 0, m.start())
        sets.append(CallSite(file=file_str, line=line, raw_id=None, is_dynamic=False))

    for m in _SAME_CLASS_CALL_RE.finditer(stripped_body):
        name = m.group(1)
        if name in {
            "if", "for", "while", "switch", "return", "sizeof",
            "static_cast", "dynamic_cast", "reinterpret_cast", "const_cast",
            "qtTrId", "QString", "qDebug", "qWarning", "qInfo", "qCritical",
            "qFatal", "QT_TRID_NOOP", "Q_EMIT", "emit", "Q_UNUSED",
            "Q_ASSERT", "Q_OBJECT", "tr",
        }:
            continue
        calls.add(name)

    return qtt, sets, calls


def _scan_change_event_branches(stripped_body: str) -> set[str]:
    """Heuristic: detect QEvent::* branches handled in changeEvent body.

    Looks for `QEvent::LanguageChange`, `QEvent::PaletteChange`, etc.
    """

    branches: set[str] = set()
    for m in re.finditer(r"\bQEvent::(\w+)\b", stripped_body):
        branches.add(m.group(1))
    return branches


def _index_file(
    path: Path, classes: dict[str, ClassRecord], rel_to: Path
) -> None:
    """Parse a single source file and update the class registry in place."""

    try:
        text = path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return
    stripped = strip_comments_and_strings(text)
    file_rel = str(path.relative_to(rel_to)) if rel_to in path.parents or path == rel_to else str(path)

    # Class declarations (header-side primary).
    for m in _CLASS_DECL_RE.finditer(stripped):
        cls_name = m.group(1)
        rec = classes.setdefault(cls_name, ClassRecord(name=cls_name))
        if path.suffix in (".h", ".hpp", ".hh") and rec.header is None:
            rec.header = path
        if file_rel not in rec.declaring_files:
            rec.declaring_files.append(file_rel)

    def _capture(cls_name: str, method_name: str, brace_idx: int) -> None:
        end_idx = _find_matching_brace(stripped, brace_idx)
        if end_idx == -1:
            return
        body_stripped = stripped[brace_idx + 1 : end_idx]
        body_original = text[brace_idx + 1 : end_idx]
        start_line = _line_number(stripped, brace_idx)
        end_line = _line_number(stripped, end_idx)
        qtt, sets, calls = _scan_callsites_with_originals(
            file_rel, body_original, body_stripped, start_line
        )
        body = MethodBody(
            cls=cls_name,
            name=method_name,
            file=file_rel,
            start_line=start_line,
            end_line=end_line,
            text=body_stripped,
            qttrid_callsites=qtt,
            setter_callsites=sets,
            same_class_calls=calls,
        )
        rec = classes.setdefault(cls_name, ClassRecord(name=cls_name))
        if file_rel not in rec.declaring_files:
            rec.declaring_files.append(file_rel)
        if path not in rec.impl_files and path.suffix in (".cpp", ".cc", ".cxx"):
            rec.impl_files.append(path)
        # Multiple ctor overloads share the synthetic key "<ctor>";
        # merge their callsites instead of overwriting.
        if method_name in rec.methods:
            prev = rec.methods[method_name]
            prev.qttrid_callsites.extend(body.qttrid_callsites)
            prev.setter_callsites.extend(body.setter_callsites)
            prev.same_class_calls |= body.same_class_calls
        else:
            rec.methods[method_name] = body
        if method_name == "retranslateUi":
            rec.has_retranslate_ui = True
        if method_name == "changeEvent":
            rec.change_event_branches |= _scan_change_event_branches(body_stripped)

    # Method definitions: `RetType ClassName::method(...) { ... }`
    for m in _METHOD_DEF_RE.finditer(stripped):
        cls_name = m.group(1)
        method_name = m.group(2)
        # Skip when this overlaps a ctor/dtor pattern (handled below).
        if cls_name == method_name:
            continue
        _capture(cls_name, method_name, m.end() - 1)

    # Constructors: `ClassName::ClassName(...) [: init...] { ... }`
    for m in _CTOR_DEF_RE.finditer(stripped):
        cls_name = m.group(1)
        _capture(cls_name, "<ctor>", m.end() - 1)

    # Destructors: `ClassName::~ClassName() { ... }`
    for m in _DTOR_DEF_RE.finditer(stripped):
        cls_name = m.group(1)
        _capture(cls_name, "<dtor>", m.end() - 1)

    # Header-declared retranslateUi / changeEvent overrides (no body in
    # header is fine; we already scanned bodies in .cpp). But if a
    # header inlines the body (rare), capture too.
    # Detect `void retranslateUi(...)` declarations in headers (so D1
    # passes for classes that declare-but-define-elsewhere).
    if path.suffix in (".h", ".hpp", ".hh"):
        # Walk header classes and look at member function declarations
        # via a coarse search: for each class block, collect declarations
        # of retranslateUi / changeEvent.
        for m in _CLASS_DECL_RE.finditer(stripped):
            cls_name = m.group(1)
            brace_idx = m.end() - 1
            end_idx = _find_matching_brace(stripped, brace_idx)
            if end_idx == -1:
                continue
            body = stripped[brace_idx + 1 : end_idx]
            rec = classes.setdefault(cls_name, ClassRecord(name=cls_name))
            if rec.header is None:
                rec.header = path
            if file_rel not in rec.declaring_files:
                rec.declaring_files.append(file_rel)
            if re.search(r"\bretranslateUi\s*\(", body):
                rec.has_retranslate_ui = True
            if re.search(r"\bchangeEvent\s*\(", body):
                # presence of declaration in header; branches recorded
                # from .cpp body. If the body is inlined here, also
                # detect the LanguageChange branch.
                rec.change_event_branches |= _scan_change_event_branches(body)


def index_sources(roots: Sequence[Path], rel_to: Path) -> dict[str, ClassRecord]:
    """Walk source roots and return the class registry."""

    classes: dict[str, ClassRecord] = {}
    for root in roots:
        for path in sorted(root.rglob("*")):
            if path.is_dir():
                continue
            if path.suffix not in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"):
                continue
            _index_file(path, classes, rel_to)
    return classes


# --------------------------------------------------------------------------
# Allowlist (A.S.1, used by detectors)
# --------------------------------------------------------------------------

_ALLOWLIST_LINE_RE = re.compile(r"^([^:]+):(D\d+):(.+)$")


class AllowlistError(Exception):
    """Raised on malformed allowlist syntax (translates to exit 2)."""


def load_allowlist(path: Path) -> list[AllowlistEntry]:
    if not path.is_file():
        return []
    entries: list[AllowlistEntry] = []
    for lineno, raw in enumerate(path.read_text(encoding="utf-8").splitlines(), start=1):
        s = raw.strip()
        if not s or s.startswith("#"):
            continue
        m = _ALLOWLIST_LINE_RE.match(s)
        if not m:
            raise AllowlistError(
                f"{path}:{lineno}: malformed allowlist line: {raw!r} "
                f"(expected `<path>:D<n>:<reason>`)"
            )
        file_part = m.group(1).strip()
        dim_part = m.group(2).strip()
        reason = m.group(3).strip()
        if not file_part or not reason:
            raise AllowlistError(
                f"{path}:{lineno}: file or reason missing in allowlist line"
            )
        entries.append(AllowlistEntry(file=file_part, dim=dim_part, reason=reason))
    return entries


def _inline_allowlist_dim_for_line(file_text: str, line_no: int) -> str | None:
    """Return the allowlisted dim if the given 1-based line carries an
    inline `// i18n-audit: ignore Dn, reason` annotation; else None.

    Reason text is mandatory; a comment without reason raises
    AllowlistError (exit 2).
    """
    lines = file_text.splitlines()
    if not (1 <= line_no <= len(lines)):
        return None
    line = lines[line_no - 1]
    # Find any '// i18n-audit:' substring on the line. Reject the loose
    # form (no comma + reason) explicitly.
    if "i18n-audit" not in line:
        return None
    # Strict regex on the trailing comment only.
    cidx = line.rfind("//")
    if cidx == -1:
        return None
    comment = line[cidx:].strip()
    m = _INLINE_ALLOWLIST_RE.match(comment)
    if not m:
        # Looks like an attempt at the annotation; reject.
        if "i18n-audit" in comment and "ignore" in comment:
            raise AllowlistError(
                f"inline allowlist annotation missing reason text on line "
                f"{line_no}: {comment!r} "
                f"(canonical form: `// i18n-audit: ignore Dn, reason`)"
            )
        return None
    return m.group(1)


# --------------------------------------------------------------------------
# Detectors (A.S.3 + A.S.4)
# --------------------------------------------------------------------------


def _emit(
    findings: list[Finding],
    *,
    dim: str,
    file: str,
    line: int,
    cls: str | None,
    message: str,
) -> None:
    findings.append(
        Finding(
            file=file,
            line=line,
            dim=dim,
            severity=SEVERITY[dim],
            cls=cls,
            message=message,
            fix_hint=FIX_HINTS[dim],
        )
    )


def detect_d1(reg: Mapping[str, ClassRecord]) -> list[Finding]:
    """D1 — class uses qtTrId() somewhere but lacks both retranslateUi()
    and a changeEvent override that handles QEvent::LanguageChange."""

    out: list[Finding] = []
    for cls_name, rec in reg.items():
        # Collect every qtTrId callsite across the class.
        cs: list[CallSite] = []
        for body in rec.methods.values():
            cs.extend(body.qttrid_callsites)
        cs.extend(rec.extra_qttrid_callsites)
        if not cs:
            continue
        if rec.has_retranslate_ui:
            continue
        if "LanguageChange" in rec.change_event_branches:
            continue
        # Some widgets handle LanguageChange via QObject::event() rather
        # than QWidget::changeEvent(). Treat any LanguageChange branch
        # in any method body as evidence of a retranslate path.
        any_lang_branch = any(
            "LanguageChange" in _scan_change_event_branches(b.text)
            for b in rec.methods.values()
        )
        if any_lang_branch:
            continue
        # Emit at the first callsite location for stable sort.
        first = min(cs, key=lambda c: (c.file, c.line))
        _emit(
            out,
            dim="D1",
            file=first.file,
            line=first.line,
            cls=cls_name,
            message=(
                f"{cls_name} calls qtTrId() but defines no retranslateUi() "
                f"and no changeEvent(LanguageChange) branch"
            ),
        )
    return out


def _retranslate_closure(rec: ClassRecord, entry: str = "retranslateUi") -> set[str]:
    """Set of method names transitively reached from `entry` via
    same-class call edges."""

    closure: set[str] = set()
    if entry not in rec.methods:
        return closure
    stack = [entry]
    while stack:
        cur = stack.pop()
        if cur in closure:
            continue
        closure.add(cur)
        body = rec.methods.get(cur)
        if body is None:
            continue
        for nxt in body.same_class_calls:
            if nxt in rec.methods and nxt not in closure:
                stack.append(nxt)
    return closure


def detect_d2(reg: Mapping[str, ClassRecord]) -> list[Finding]:
    """D2 — retranslateUi() exists, but a same-class qtTrId callsite is
    not transitively reachable from retranslateUi().

    Emits at most one finding per (class, method) so the output reflects
    the natural remediation unit (one method to wire in or factor out).
    The finding line points at the first uncovered qtTrId in the method
    body; the message names the class, the method, and the count of
    affected callsites so reviewers can see the magnitude.
    """

    out: list[Finding] = []
    for cls_name, rec in reg.items():
        if not rec.has_retranslate_ui:
            continue
        closure = _retranslate_closure(rec, "retranslateUi")
        if not closure:
            continue
        for method_name, body in rec.methods.items():
            if method_name in closure:
                continue
            uncovered = body.qttrid_callsites
            if not uncovered:
                continue
            first = min(uncovered, key=lambda c: (c.file, c.line))
            _emit(
                out,
                dim="D2",
                file=first.file,
                line=first.line,
                cls=cls_name,
                message=(
                    f"{cls_name}::{method_name}() calls qtTrId() in "
                    f"{len(uncovered)} place(s) but is not reached "
                    f"transitively from retranslateUi()"
                ),
            )
    return out


def detect_d3(file_paths: Iterable[Path], rel_to: Path) -> list[Finding]:
    """D3 — static QString s = qtTrId(...) (or static const QString s,
    later assigned to qtTrId(...))."""

    out: list[Finding] = []
    for path in file_paths:
        if path.suffix not in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        stripped = strip_comments_and_strings(text)
        rel = str(path.relative_to(rel_to)) if rel_to in path.parents or path == rel_to else str(path)

        for m in _D3_INLINE_STATIC_RE.finditer(stripped):
            line = _line_number(stripped, m.start())
            _emit(
                out,
                dim="D3",
                file=rel,
                line=line,
                cls=None,
                message=(
                    "static-local qtTrId() snapshot: the first call freezes "
                    "the translation forever; reload after language switch fails"
                ),
            )
        for m in _D3_DECL_RE.finditer(stripped):
            var = m.group(1)
            # Look forward for assignment to qtTrId within the same scope.
            tail = stripped[m.end() :]
            assign_re = re.compile(rf"\b{re.escape(var)}\s*=\s*qtTrId\b")
            am = assign_re.search(tail)
            if am:
                line = _line_number(stripped, m.end() + am.start())
                _emit(
                    out,
                    dim="D3",
                    file=rel,
                    line=line,
                    cls=None,
                    message=(
                        f"static QString {var} later assigned via qtTrId(): "
                        "first-call value persists across language switches"
                    ),
                )
    return out


def detect_d8(reg: Mapping[str, ClassRecord]) -> list[Finding]:
    """D8 — setToolTip / setAccessibleName / setStatusTip / setWhatsThis
    with qtTrId outside the retranslateUi closure.

    Emits at most one finding per (class, method); message includes the
    callsite count.
    """

    out: list[Finding] = []
    for cls_name, rec in reg.items():
        closure = _retranslate_closure(rec, "retranslateUi") if rec.has_retranslate_ui else set()
        for method_name, body in rec.methods.items():
            if method_name in closure:
                continue
            if not body.setter_callsites:
                continue
            first = min(body.setter_callsites, key=lambda c: (c.file, c.line))
            _emit(
                out,
                dim="D8",
                file=first.file,
                line=first.line,
                cls=cls_name,
                message=(
                    f"{cls_name}::{method_name}() applies "
                    f"setToolTip/setAccessibleName/setStatusTip/setWhatsThis "
                    f"with qtTrId() in {len(body.setter_callsites)} place(s) "
                    f"outside retranslateUi()"
                ),
            )
    return out


# --------------------------------------------------------------------------
# Catalog (D5, D6) — A.S.4
# --------------------------------------------------------------------------


@dataclass
class CatalogIndex:
    en_ids: set[str]
    sr_ids: set[str]
    catalog_noop_ids: set[str]
    dynamic_id_patterns: list[str]  # regex patterns derived from unresolved callsites


def _ts_ids(path: Path) -> set[str]:
    """Return the set of `<message id="...">` IDs in a Qt .ts file."""

    if not path.is_file():
        return set()
    try:
        tree = ET.parse(path)
    except ET.ParseError:
        return set()
    ids: set[str] = set()
    for msg in tree.getroot().iter("message"):
        mid = msg.attrib.get("id")
        if mid:
            ids.add(mid)
    return ids


def _catalog_noop_ids(catalog_path: Path) -> set[str]:
    if not catalog_path.is_file():
        return set()
    try:
        text = catalog_path.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        return set()
    stripped = strip_comments_and_strings(text)
    out: set[str] = set()
    # Anchor: each NOOP callsite must remain visible after comment/string
    # stripping. Recover the literal id from the un-stripped text at the
    # matching offset.
    noop_any_re = re.compile(r"\bQT_TRID_NOOP\(\s*([^)]*?)\)")
    for m in noop_any_re.finditer(stripped):
        if stripped[m.start() : m.start() + len("QT_TRID_NOOP(")] != "QT_TRID_NOOP(":
            continue
        lm = _QT_TRID_NOOP_RE.match(text, m.start())
        if lm is not None:
            out.add(lm.group(1))
    return out


def load_catalog(en_ts: Path, sr_ts: Path, noop_cpp: Path) -> CatalogIndex:
    return CatalogIndex(
        en_ids=_ts_ids(en_ts),
        sr_ids=_ts_ids(sr_ts),
        catalog_noop_ids=_catalog_noop_ids(noop_cpp),
        dynamic_id_patterns=[],
    )


def _all_noop_ids_in_sources(
    file_paths: Iterable[Path], exclude_paths: Iterable[Path] = ()
) -> set[str]:
    """Scan every source file (except `exclude_paths`) for
    QT_TRID_NOOP("lc-...") callsites.

    Consumer-side QT_TRID_NOOP forms (e.g.
    `static const char* const trIdStep = QT_TRID_NOOP("lc-step")`) are
    counted as "consumes" so D6 does not flag them as orphans even
    though they may only be looked up later via qtTrId(constant).
    The canonical `translations_catalog.cpp` is excluded from this scan
    via `exclude_paths`; it is the *source* of catalog-side IDs whose
    consumers we are precisely looking for here.
    """

    excl = {p.resolve() for p in exclude_paths if p}
    ids: set[str] = set()
    for path in file_paths:
        if path.suffix not in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"):
            continue
        if path.resolve() in excl:
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        stripped = strip_comments_and_strings(text)
        noop_any_re = re.compile(r"\bQT_TRID_NOOP\(\s*([^)]*?)\)")
        for m in noop_any_re.finditer(stripped):
            if stripped[m.start() : m.start() + len("QT_TRID_NOOP(")] != "QT_TRID_NOOP(":
                continue
            lm = _QT_TRID_NOOP_RE.match(text, m.start())
            if lm is not None:
                ids.add(lm.group(1))
    return ids


def _all_qttrid_ids(reg: Mapping[str, ClassRecord]) -> tuple[set[str], list[CallSite]]:
    """Return (resolved literal IDs, dynamic callsites)."""

    ids: set[str] = set()
    dyns: list[CallSite] = []
    for rec in reg.values():
        for body in rec.methods.values():
            for cs in body.qttrid_callsites:
                if cs.raw_id is not None:
                    ids.add(cs.raw_id)
                else:
                    dyns.append(cs)
        for cs in rec.extra_qttrid_callsites:
            if cs.raw_id is not None:
                ids.add(cs.raw_id)
            else:
                dyns.append(cs)
    return ids, dyns


def _free_floating_qttrid(file_paths: Iterable[Path], rel_to: Path) -> tuple[set[str], list[tuple[str, int]]]:
    """Scan all source files for *any* qtTrId callsite, anchored by the
    stripped text so commented-out callsites do not register. Returns
    (literal_ids, dynamic_callsite_locs)."""

    ids: set[str] = set()
    dyns: list[tuple[str, int]] = []
    for path in file_paths:
        if path.suffix not in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        stripped = strip_comments_and_strings(text)
        rel = str(path.relative_to(rel_to)) if rel_to in path.parents or path == rel_to else str(path)
        for m in _QTTRID_ANY_RE.finditer(stripped):
            if stripped[m.start() : m.start() + len("qtTrId(")] != "qtTrId(":
                continue
            lm = _QTTRID_LITERAL_RE.match(text, m.start())
            if lm is not None:
                ids.add(lm.group(1))
            else:
                dyns.append((rel, _line_number(stripped, m.start())))
    return ids, dyns


def detect_d5(
    reg: Mapping[str, ClassRecord],
    file_paths: Iterable[Path],
    rel_to: Path,
    cat: CatalogIndex,
) -> list[Finding]:
    """D5 — qtTrId ID with no matching <message id="..."> in en.ts /
    sr_RS.ts, or with no QT_TRID_NOOP entry in translations_catalog.cpp.
    Reports each missing piece once per source ID."""

    out: list[Finding] = []
    file_paths = list(file_paths)
    free_ids, _ = _free_floating_qttrid(file_paths, rel_to)
    class_ids, _ = _all_qttrid_ids(reg)
    src_ids = free_ids | class_ids

    # Map each ID back to a representative (file, line) for reporting:
    # walk the files once and keep first-seen.
    id_locations: dict[str, tuple[str, int]] = {}
    for path in file_paths:
        if path.suffix not in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"):
            continue
        try:
            text = path.read_text(encoding="utf-8")
        except (OSError, UnicodeDecodeError):
            continue
        stripped = strip_comments_and_strings(text)
        rel = str(path.relative_to(rel_to)) if rel_to in path.parents or path == rel_to else str(path)
        for m in _QTTRID_ANY_RE.finditer(stripped):
            if stripped[m.start() : m.start() + len("qtTrId(")] != "qtTrId(":
                continue
            lm = _QTTRID_LITERAL_RE.match(text, m.start())
            if lm is None:
                continue
            mid = lm.group(1)
            if mid in id_locations:
                continue
            id_locations[mid] = (rel, _line_number(stripped, m.start()))

    # Sort to keep findings deterministic.
    for mid in sorted(src_ids):
        loc = id_locations.get(mid)
        if loc is None:
            # Came from a header declaration we couldn't trace; emit at line 1.
            loc = ("?", 1)
        rel, line = loc
        missing: list[str] = []
        if mid not in cat.catalog_noop_ids:
            missing.append("translations_catalog.cpp QT_TRID_NOOP")
        if mid not in cat.en_ids:
            missing.append("LibreCelik_en.ts")
        if mid not in cat.sr_ids:
            missing.append("LibreCelik_sr_RS.ts")
        if not missing:
            continue
        _emit(
            out,
            dim="D5",
            file=rel,
            line=line,
            cls=None,
            message=(
                f"qtTrId(\"{mid}\") missing from: " + ", ".join(missing)
            ),
        )
    return out


def detect_d6(
    reg: Mapping[str, ClassRecord],
    file_paths: Iterable[Path],
    rel_to: Path,
    cat: CatalogIndex,
    catalog_cpp: Path,
) -> list[Finding]:
    """D6 — orphan: catalog ID with no source consumer.

    Conservative: when any unresolved qtTrId(EXPR) callsite exists, we
    treat IDs matching the prefix `lc-` as potentially produced by it
    and refuse to flag them. (The actual prefix set is parameterised
    via the dynamic_id_patterns list, populated below.)
    """

    out: list[Finding] = []
    file_paths = list(file_paths)
    free_ids, _ = _free_floating_qttrid(file_paths, rel_to)
    class_ids, class_dyns = _all_qttrid_ids(reg)
    src_ids = free_ids | class_ids
    # Consumer-side NOOPs reserve IDs that get looked up dynamically
    # later (e.g. wizard-step constants). Exclude the canonical
    # translations_catalog.cpp so its own NOOP entries do not silence
    # orphan-detection of themselves.
    src_ids |= _all_noop_ids_in_sources(file_paths, exclude_paths=[catalog_cpp])
    _, free_dyns = _free_floating_qttrid(file_paths, rel_to)
    has_dynamic = bool(class_dyns) or bool(free_dyns)

    catalog_ids = cat.en_ids | cat.sr_ids | cat.catalog_noop_ids
    orphans = sorted(catalog_ids - src_ids)

    # Locate orphans: report against translations_catalog.cpp (or .ts if
    # NOOP-missing too). We point at the catalog file with line 1 as a
    # stable anchor — file-level rather than message-level.
    for mid in orphans:
        # Conservative suppression: when any unresolvable qtTrId(EXPR)
        # callsite exists in the source tree, IDs that match the lc-*
        # prefix could be produced dynamically. We already credit
        # consumer-side QT_TRID_NOOP registrations; but if the caller
        # builds the id at runtime via QString::arg() etc., no NOOP
        # exists. Suppress the warning in that case (Spec §6.4 D6).
        if has_dynamic and mid.startswith("lc-"):
            continue
        anchor_file = "src/utils/translations_catalog.cpp"
        anchor_line = 1
        _emit(
            out,
            dim="D6",
            file=anchor_file,
            line=anchor_line,
            cls=None,
            message=(
                f"orphan catalog ID \"{mid}\": present in en.ts / sr_RS.ts / "
                f"NOOP catalog but no source-side qtTrId() / QT_TRID_NOOP "
                f"references it"
            ),
        )
    return out


# --------------------------------------------------------------------------
# Allowlist filtering
# --------------------------------------------------------------------------


def _file_text_cache(file: str, rel_to: Path, cache: dict[str, str]) -> str | None:
    if file in cache:
        return cache[file]
    p = (rel_to / file)
    if not p.is_file():
        cache[file] = ""
        return None
    try:
        cache[file] = p.read_text(encoding="utf-8")
    except (OSError, UnicodeDecodeError):
        cache[file] = ""
        return None
    return cache[file]


def apply_allowlists(
    findings: list[Finding],
    file_entries: Sequence[AllowlistEntry],
    rel_to: Path,
) -> list[Finding]:
    """Mark allowlisted findings (file-level + inline) by replacing
    them with copies that have allowlisted=True. Returns a new list."""

    file_map: dict[tuple[str, str], AllowlistEntry] = {
        (e.file, e.dim): e for e in file_entries
    }
    text_cache: dict[str, str] = {}
    out: list[Finding] = []
    for f in findings:
        allowed = False
        if (f.file, f.dim) in file_map:
            allowed = True
        if not allowed:
            text = _file_text_cache(f.file, rel_to, text_cache)
            if text is not None:
                inline_dim = _inline_allowlist_dim_for_line(text, f.line)
                if inline_dim is not None and inline_dim == f.dim:
                    allowed = True
        out.append(
            Finding(
                file=f.file,
                line=f.line,
                dim=f.dim,
                severity=f.severity,
                cls=f.cls,
                message=f.message,
                fix_hint=f.fix_hint,
                allowlisted=allowed,
            )
        )
    return out


# --------------------------------------------------------------------------
# Output writers
# --------------------------------------------------------------------------


def write_human(out: IO[str], findings: Sequence[Finding], summary: dict) -> None:
    out.write(f"LibreCelik i18n audit (tool {__version__})\n")
    out.write("=" * 36 + "\n")
    by_dim: dict[str, list[Finding]] = defaultdict(list)
    for f in findings:
        by_dim[f.dim].append(f)
    for dim in ALL_DIMS:
        bucket = by_dim.get(dim, [])
        active = [f for f in bucket if not f.allowlisted]
        if not bucket:
            continue
        out.write(
            f"{dim} — {SEVERITY[dim]:<6}: {len(active)} findings"
            f" ({len(bucket) - len(active)} allowlisted)\n"
        )
        for f in sorted(bucket):
            tag = "  [allowlisted] " if f.allowlisted else "  "
            out.write(f"{tag}{f.file}:{f.line}  {f.message}\n")
            out.write(f"    fix: {f.fix_hint}\n")
        out.write("\n")
    total_active = sum(1 for f in findings if not f.allowlisted)
    total_allowlisted = sum(1 for f in findings if f.allowlisted)
    out.write(
        f"Total: {total_active} findings ({total_allowlisted} allowlisted), "
        f"exit {0 if total_active == 0 else 1}\n"
    )


def write_json(out: IO[str], findings: Sequence[Finding], summary: dict) -> None:
    # Note: no `timestamp` field — determinism (Spec I1) requires byte-for-byte
    # identical output across runs on the same input. Wall-clock time is a
    # property of the run, not of the audit; CI captures it externally.
    payload = {
        "schema_version": SCHEMA_VERSION,
        "tool": "i18n-audit",
        "tool_version": __version__,
        "dimensions_run": summary["dimensions_run"],
        "findings": [
            {
                "dim": f.dim,
                "severity": f.severity,
                "file": f.file,
                "line": f.line,
                "class": f.cls,
                "message": f.message,
                "fix_hint": f.fix_hint,
                "allowlisted": f.allowlisted,
            }
            for f in sorted(findings)
        ],
        "summary": {
            "total": sum(1 for f in findings if not f.allowlisted),
            "allowlisted": sum(1 for f in findings if f.allowlisted),
            "by_dim": {
                dim: sum(1 for f in findings if f.dim == dim and not f.allowlisted)
                for dim in ALL_DIMS
                if any(f.dim == dim for f in findings)
            },
        },
    }
    json.dump(payload, out, indent=2, sort_keys=False)
    out.write("\n")


# --------------------------------------------------------------------------
# Audit driver
# --------------------------------------------------------------------------


def _all_source_files(roots: Sequence[Path]) -> list[Path]:
    out: list[Path] = []
    for root in roots:
        if not root.exists():
            continue
        for p in sorted(root.rglob("*")):
            if p.is_file() and p.suffix in (".cpp", ".cc", ".cxx", ".h", ".hpp", ".hh"):
                out.append(p)
    return out


def _diff_since_files(ref: str, repo_root: Path) -> list[Path] | None:
    try:
        proc = subprocess.run(
            ["git", "diff", "--name-only", ref, "--"],
            cwd=repo_root,
            capture_output=True,
            text=True,
            check=True,
        )
    except (FileNotFoundError, subprocess.CalledProcessError):
        return None
    out: list[Path] = []
    for line in proc.stdout.splitlines():
        p = repo_root / line.strip()
        if p.is_file():
            out.append(p)
    return out


def run_audit(
    *,
    src_roots: Sequence[Path],
    en_ts: Path,
    sr_ts: Path,
    catalog_cpp: Path,
    rel_to: Path,
    dims: Sequence[str],
    diff_files: Sequence[Path] | None = None,
) -> tuple[list[Finding], dict]:
    """Run the audit. Returns (findings, summary)."""

    classes = index_sources(src_roots, rel_to)
    all_files = _all_source_files(src_roots)
    catalog = load_catalog(en_ts, sr_ts, catalog_cpp)

    findings: list[Finding] = []
    if "D1" in dims:
        findings.extend(detect_d1(classes))
    if "D2" in dims:
        findings.extend(detect_d2(classes))
    if "D3" in dims:
        findings.extend(detect_d3(all_files, rel_to))
    if "D8" in dims:
        findings.extend(detect_d8(classes))
    if "D5" in dims:
        findings.extend(detect_d5(classes, all_files, rel_to, catalog))
    if "D6" in dims:
        findings.extend(detect_d6(classes, all_files, rel_to, catalog, catalog_cpp))

    if diff_files is not None:
        diff_set = {
            str(p.relative_to(rel_to))
            if rel_to in p.parents or p == rel_to
            else str(p)
            for p in diff_files
        }
        findings = [f for f in findings if f.file in diff_set]

    findings.sort()
    summary = {
        "dimensions_run": list(dims),
    }
    return findings, summary


# --------------------------------------------------------------------------
# CLI
# --------------------------------------------------------------------------


def parse_args(argv: Sequence[str]) -> argparse.Namespace:
    p = argparse.ArgumentParser(
        prog="i18n_audit",
        description="LibreCelik i18n retranslate audit (static checker).",
    )
    p.add_argument(
        "--dims",
        default=",".join(ALL_DIMS),
        help="Comma-separated list of dimensions to run (default: all).",
    )
    p.add_argument("--json", dest="json_path", default=None, help="Write JSON output to PATH.")
    p.add_argument(
        "--strict",
        action="store_true",
        help="Exit 1 on first finding (after allowlist filtering).",
    )
    p.add_argument(
        "--diff-since",
        dest="diff_since",
        default=None,
        help="Audit only files changed since git REF.",
    )
    p.add_argument(
        "--repo-root",
        dest="repo_root",
        default=None,
        help="Repo root (default: parent of tools/ where the script lives).",
    )
    p.add_argument(
        "--allowlist",
        dest="allowlist",
        default=None,
        help="Path to file-level allowlist (default: <repo>/tools/i18n_audit.allowlist).",
    )
    p.add_argument(
        "--src-root",
        action="append",
        dest="src_roots",
        default=None,
        help="Source root to scan (repeatable). Default: src/ and plugins/.",
    )
    p.add_argument("--version", action="version", version=f"i18n-audit {__version__}")
    return p.parse_args(list(argv))


def _resolve_dims(spec: str) -> list[str]:
    raw = [s.strip() for s in spec.split(",") if s.strip()]
    out: list[str] = []
    for d in raw:
        if d not in ALL_DIMS:
            raise SystemExit(f"i18n-audit: unknown dimension {d!r}; valid: {', '.join(ALL_DIMS)}")
        if d not in out:
            out.append(d)
    return out


def main(argv: Sequence[str] | None = None) -> int:
    argv = list(sys.argv[1:] if argv is None else argv)
    args = parse_args(argv)

    if args.repo_root is not None:
        repo_root = Path(args.repo_root).resolve()
    else:
        # script lives at <repo>/tools/i18n_audit.py
        repo_root = Path(__file__).resolve().parent.parent

    if args.src_roots:
        src_roots = [Path(r).resolve() for r in args.src_roots]
    else:
        src_roots = [repo_root / "src", repo_root / "plugins"]
    en_ts = repo_root / "resources" / "i18n" / "LibreCelik_en.ts"
    sr_ts = repo_root / "resources" / "i18n" / "LibreCelik_sr_RS.ts"
    catalog_cpp = repo_root / "src" / "utils" / "translations_catalog.cpp"
    allowlist_path = (
        Path(args.allowlist).resolve()
        if args.allowlist
        else repo_root / "tools" / "i18n_audit.allowlist"
    )

    try:
        dims = _resolve_dims(args.dims)
    except SystemExit as e:
        print(str(e), file=sys.stderr)
        return 2

    try:
        file_entries = load_allowlist(allowlist_path)
    except AllowlistError as e:
        print(f"i18n-audit: {e}", file=sys.stderr)
        return 2

    diff_files: list[Path] | None = None
    if args.diff_since:
        diff_files = _diff_since_files(args.diff_since, repo_root)
        if diff_files is None:
            print(
                f"i18n-audit: --diff-since {args.diff_since!r} failed (not a git repo "
                "or invalid ref)",
                file=sys.stderr,
            )
            return 2

    try:
        findings, summary = run_audit(
            src_roots=src_roots,
            en_ts=en_ts,
            sr_ts=sr_ts,
            catalog_cpp=catalog_cpp,
            rel_to=repo_root,
            dims=dims,
            diff_files=diff_files,
        )
    except AllowlistError as e:
        print(f"i18n-audit: {e}", file=sys.stderr)
        return 2

    try:
        findings = apply_allowlists(findings, file_entries, repo_root)
    except AllowlistError as e:
        print(f"i18n-audit: {e}", file=sys.stderr)
        return 2

    findings.sort()

    write_human(sys.stdout, findings, summary)
    if args.json_path:
        Path(args.json_path).write_text(
            _json_string(findings, summary), encoding="utf-8"
        )

    active = [f for f in findings if not f.allowlisted]
    if args.strict and active:
        return 1
    if active:
        return 1
    return 0


def _json_string(findings: Sequence[Finding], summary: dict) -> str:
    """Produce the JSON payload as a string (used by --json)."""
    import io

    buf = io.StringIO()
    write_json(buf, findings, summary)
    return buf.getvalue()


if __name__ == "__main__":
    sys.exit(main())
