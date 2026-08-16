# Contributing to LibreCelik

Thank you for your interest in contributing to LibreCelik. This document
captures the conventions a contributor must follow before opening a pull
request.

## Code formatting

LibreCelik uses **`clang-format` version 21** as the canonical formatter.
CI lints `src/`, `test/`, and `plugins/` against this exact version (see
`.github/workflows/ci.yml`, `format-check` job). Newer versions of
`clang-format` may emit slightly different layout decisions and produce
false positives or false negatives against CI.

To match CI locally:

- **Debian/Ubuntu**: `apt-get install clang-format-21`
  (use the LLVM nightly apt repo if 21 is not in your distribution)
- **Arch / Manjaro**: `pacman -S clang21`
- **macOS (Homebrew)**: `brew install llvm@21`, then use
  `$(brew --prefix llvm@21)/bin/clang-format`
- **Other distros / fallback**: download the prebuilt LLVM 21 release from
  <https://github.com/llvm/llvm-project/releases> and put the binary on
  `$PATH` as `clang-format-21`.

Run before every commit:

```bash
find src test plugins -name "*.cpp" -o -name "*.h" \
  | xargs clang-format-21 -i
```

If your local `clang-format` is a different major version, please install
version 21 specifically; do not commit a layout produced by another version
of the tool.

### Optional pre-commit hook

`scripts/pre-commit-clang-format.sh` runs `clang-format-21 --dry-run --Werror`
against every staged C/C++ file and rejects the commit if any file is not
formatted. Activate via:

```bash
# Per-clone (simplest):
ln -s ../../scripts/pre-commit-clang-format.sh .git/hooks/pre-commit
```

The hook respects `$CLANG_FORMAT`, otherwise tries `clang-format-21` on
PATH then `~/.local/bin/clang-format-21`. Skip a single commit with
`--no-verify`; CI will still enforce on every PR.

A repo-root `.editorconfig` documents the indent / EOL / charset
conventions for editors that support EditorConfig.

## i18n discipline (runtime retranslation)

LibreCelik supports live language switching: the user changes the
locale in Settings and every visible widget retranslates immediately,
without an application restart. Two infrastructures protect this
property:

1. **Static audit** — `tools/i18n_audit.py` scans the source tree for
   classes that call `qtTrId()` but lack a working
   `changeEvent(LanguageChange)` → `retranslateUi()` path, and for
   `qtTrId` callsites outside the `retranslateUi()` transitive
   closure (D1, D2, D3, D5, D6, D8). Run locally:

   ```bash
   python3 tools/i18n_audit.py --strict
   ```

   Exit 0 = no findings; exit 1 = active findings; exit 2 = tool
   error. Documented end-to-end in
   [`tools/README-i18n-audit.md`](tools/README-i18n-audit.md).

2. **Runtime tests** —
   `test/i18n_settings_state_test.cpp` and
   `test/i18n_plugin_retranslate_test.cpp` instantiate live
   widgets, switch translator at runtime, and verify every
   translatable string actually changed. Both run with
   `QT_QPA_PLATFORM=offscreen`.

### When you add a `qtTrId(...)` callsite

You **must**:

- ensure the call is reachable from the class's `retranslateUi()`
  method (directly or transitively via same-class helpers);
- add the matching catalogue entry: a `QT_TRANSLATE_NOOP_UTF8`
  (or equivalent) declaration plus translations in **both**
  `resources/i18n/LibreCelik_en.ts` and `LibreCelik_sr_RS.ts`.

If a string is genuinely transient (e.g. evaluated at click time
inside a lambda for a modal QFileDialog whose lifetime ends with
`exec()`), annotate the source line with the canonical inline
allowlist:

```cpp
const QString title = qtTrId("lc-foo"); // i18n-audit: ignore D2, transient file dialog — qtTrId evaluated at click time, dialog discarded after exec()
```

The reason text is mandatory. Reviewers will check that the chosen
reason matches one of the canonical phrasings recorded in
[`tools/README-i18n-audit.md`](tools/README-i18n-audit.md), the
canonical in-repo reference for the i18n retranslate-audit.

The retranslate design classifies widgets as **simple** (re-apply
`setText`), **rebuild** (clear+repopulate trees/lists), or **plugin**
(rebuild across `.so`) and is the reference for choosing a remediation
pattern; see [`tools/README-i18n-audit.md`](tools/README-i18n-audit.md)
for the canonical summary.

### Locale-stable wordlist amendments

`test/i18n_test_support/locale_stable_words.h` lists strings that
read identically in en and sr_RS by convention (acronyms like
`PIN`, `PIV`, `URL`; language endonyms `English` / `Српски`; Qt
built-in dialog button strings translated via `qt_*.qm`). Adding to
this list requires PR review — every new entry is a tacit promise
that the term is universally understood. Prefer to *translate*
instead of allowlisting whenever the receiving audience may not
recognise the English form.

### Optional pre-commit i18n hook

`scripts/pre-commit-i18n.sh` runs the audit in `--diff-since=HEAD`
mode (only files staged for commit). Activate via:

```bash
# Per-clone (simplest):
ln -s ../../scripts/pre-commit-i18n.sh .git/hooks/pre-commit-i18n
```

…then chain it from your existing `pre-commit` hook, or set
`core.hooksPath` to a directory containing both
`pre-commit-clang-format.sh` and `pre-commit-i18n.sh`.

## Build and test

LibreCelik consumes LibreAgent (ClientQt) via CMake `FetchContent`, pinned
by `cmake/libreagent.pin`. For local development, point at a sibling
LibreAgent checkout:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_LIBREAGENT=/path/to/LibreAgent
cmake --build build -j4
```

`ctest` may not discover Qt-driven LibreCelik test suites; in that case run
the test binaries directly:

```bash
for t in build/test/LibreCelik*; do
  [ -x "$t" ] && [ -f "$t" ] && "$t"
done
```

Cap parallel build jobs to `-j4` to avoid system saturation.

## Commit conventions

- One logical change per commit. Use Conventional-Commit-style subjects
  (e.g. `lc: ...`, `signing: ...`, `plugin: ...`, `docs: ...`).
- Do not include `Co-Authored-By:` lines unless explicitly requested.

## License

LibreCelik is GPL-3.0; source files carry SPDX headers, which new files
must preserve.
