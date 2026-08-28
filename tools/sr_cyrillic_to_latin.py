#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Serbian Cyrillic -> Latin transliteration for a Qt .ts catalogue.

Regenerates `resources/i18n/LibreCelik_sr_Latn_RS.ts` from
`LibreCelik_sr_RS.ts`. Run this after every `lupdate` pass that touches the
Serbian catalogue: `lupdate` now also merges into the derived Latin-script
catalogue (it is listed in `TS_PROJECT_FILES` alongside the Cyrillic
source), which lands new source strings in it as `type="unfinished"` since
`lupdate` has no way to transliterate. Rerunning this script after
translating the new Cyrillic strings regenerates the Latin catalogue in
full, so its msgid set and `unfinished` count match the source again (see
the `i18n_audit.py` D10 check, and `CONTRIBUTING.md`'s i18n section for the
full update sequence).

Scope discipline (this is the easiest place to get this wrong):

  - Only the *text content* of <translation> and <numerusform> elements is
    transliterated. <source>, <oldsource>, attributes (id=, numerus=,
    type=), tag names, and any ASCII/Latin text already present (acronyms,
    URLs, HTML tags, format specifiers like %1/%n) pass through untouched --
    the transliteration table only maps Cyrillic Unicode codepoints, so
    anything that is not Cyrillic is mechanically left alone.
  - Cyrillic -> Latin is a 1:1, unambiguous mapping in this direction
    (nj/lj/dz digraphs are deterministic going FROM Cyrillic). The reverse
    direction is ambiguous and this script deliberately does not attempt it.
  - Digraph casing (Nj/NJ, Lj/LJ, Dz/DZ) follows the standard Serbian rule:
    a lowercase digraph letter is always the lowercase digraph; an uppercase
    digraph letter renders as Title-case (Nj/Lj/Dz) when followed by a
    lowercase Cyrillic letter (start of a capitalised word), and as full
    upper-case (NJ/LJ/DZ) otherwise (all-caps word, or last letter of a
    word/string).

Usage:
    python3 tools/sr_cyrillic_to_latin.py <input.ts> <output.ts> [--language CODE]

    # Regenerating the shipped catalogue after translating new Cyrillic
    # strings and running lupdate:
    python3 tools/sr_cyrillic_to_latin.py \\
        resources/i18n/LibreCelik_sr_RS.ts \\
        resources/i18n/LibreCelik_sr_Latn_RS.ts

Exit codes: 0 success, 1 if any un-mapped Cyrillic codepoint remains after
transliteration (printed to stderr for manual review -- never silently
dropped).
"""

from __future__ import annotations

import argparse
import re
import sys
from pathlib import Path

# --------------------------------------------------------------------------
# Transliteration table
# --------------------------------------------------------------------------

# Plain 1:1 letters (27 of the 30 Serbian Cyrillic letters; the remaining
# three -- Љ/љ, Њ/њ, Џ/џ -- are digraphs handled separately below because
# their upper-case rendering is context-sensitive).
_CYR_TO_LAT: dict[str, str] = {
    "А": "A", "Б": "B", "В": "V", "Г": "G", "Д": "D", "Ђ": "Đ", "Е": "E",
    "Ж": "Ž", "З": "Z", "И": "I", "Ј": "J", "К": "K", "Л": "L", "М": "M",
    "Н": "N", "О": "O", "П": "P", "Р": "R", "С": "S", "Т": "T", "Ћ": "Ć",
    "У": "U", "Ф": "F", "Х": "H", "Ц": "C", "Ч": "Č", "Ш": "Š",
    "а": "a", "б": "b", "в": "v", "г": "g", "д": "d", "ђ": "đ", "е": "e",
    "ж": "ž", "з": "z", "и": "i", "ј": "j", "к": "k", "л": "l", "м": "m",
    "н": "n", "о": "o", "п": "p", "р": "r", "с": "s", "т": "t", "ћ": "ć",
    "у": "u", "ф": "f", "х": "h", "ц": "c", "ч": "č", "ш": "š",
}

_DIGRAPH_LOWER: dict[str, str] = {"љ": "lj", "њ": "nj", "џ": "dž"}
_DIGRAPH_TITLE: dict[str, str] = {"Љ": "Lj", "Њ": "Nj", "Џ": "Dž"}
_DIGRAPH_UPPER: dict[str, str] = {"Љ": "LJ", "Њ": "NJ", "Џ": "DŽ"}

# Any Cyrillic codepoint outside the Serbian 30-letter alphabet (e.g. Russian
# -only letters Ё/Й/Ъ/Ы/Ь/Э/Ю/Я) has no place in a Serbian .ts file; if the
# transliteration leaves one of these (or anything else in the Cyrillic
# block) behind, that is a data anomaly worth failing loudly on rather than
# emitting mojibake.
_CYRILLIC_RANGE_RE = re.compile(r"[Ѐ-ӿ]")


def transliterate_run(text: str) -> str:
    """Transliterate a chunk of Serbian Cyrillic text to Latin.

    Non-Cyrillic characters (ASCII letters, digits, punctuation, HTML
    entities already present as literal '&amp;' etc., format specifiers)
    pass through unchanged.
    """

    out: list[str] = []
    n = len(text)
    i = 0
    while i < n:
        c = text[i]
        if c in _DIGRAPH_LOWER:
            out.append(_DIGRAPH_LOWER[c])
            i += 1
            continue
        if c in _DIGRAPH_TITLE:  # uppercase digraph letter
            nxt = text[i + 1] if i + 1 < n else ""
            if nxt.isalpha() and nxt.islower():
                out.append(_DIGRAPH_TITLE[c])
            else:
                out.append(_DIGRAPH_UPPER[c])
            i += 1
            continue
        out.append(_CYR_TO_LAT.get(c, c))
        i += 1
    return "".join(out)


# --------------------------------------------------------------------------
# .ts text-level rewrite (regex-based, NOT a full XML round-trip, so that
# byte-for-byte formatting/entity-escaping outside <translation>/
# <numerusform> is preserved exactly).
# --------------------------------------------------------------------------

# Matches an entire <translation ...>...</translation> block (which may
# contain nested <numerusform> children), DOTALL so it spans lines. Tag
# *names* and attribute syntax are pure ASCII and are therefore never
# touched by transliterate_run -- only the Cyrillic codepoints inside are
# rewritten, wherever they occur inside the captured span.
_TRANSLATION_BLOCK_RE = re.compile(r"(<translation\b[^>]*>)(.*?)(</translation>)", re.DOTALL)


def transliterate_ts_text(ts_text: str) -> str:
    def _sub(m: re.Match[str]) -> str:
        open_tag, body, close_tag = m.group(1), m.group(2), m.group(3)
        return open_tag + transliterate_run(body) + close_tag

    return _TRANSLATION_BLOCK_RE.sub(_sub, ts_text)


def convert(input_path: Path, output_path: Path, language: str) -> int:
    src = input_path.read_text(encoding="utf-8")
    out = transliterate_ts_text(src)

    # Retarget the TS root's `language` attribute to the Latin variant code.
    out, n = re.subn(
        r'(<TS\b[^>]*\blanguage=")[^"]*(")',
        rf"\g<1>{language}\g<2>",
        out,
        count=1,
    )
    if n != 1:
        print(f"error: could not find <TS ... language=\"...\"> root to retarget in {input_path}", file=sys.stderr)
        return 1

    # Manual-review safety net: fail loudly if any Cyrillic codepoint
    # survived the transliteration table anywhere in the output.
    leftover = sorted(set(_CYRILLIC_RANGE_RE.findall(out)))
    if leftover:
        print(
            f"error: {len(leftover)} un-mapped Cyrillic codepoint(s) survived "
            f"transliteration: {leftover!r} -- extend the mapping table and "
            f"rerun; refusing to write a half-transliterated catalogue",
            file=sys.stderr,
        )
        return 1

    output_path.write_text(out, encoding="utf-8")
    return 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input_ts", type=Path)
    p.add_argument("output_ts", type=Path)
    p.add_argument("--language", default="sr_Latn_RS", help="TS root language attribute for the output (default: sr_Latn_RS)")
    args = p.parse_args(argv)
    return convert(args.input_ts, args.output_ts, args.language)


if __name__ == "__main__":
    sys.exit(main())
