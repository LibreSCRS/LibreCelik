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

## Build and test

LibreCelik consumes LibreMiddleware via CMake `FetchContent`. For local
development, point at a sibling LibreMiddleware checkout:

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
      -DFETCHCONTENT_SOURCE_DIR_LIBREMIDDLEWARE=/path/to/LibreMiddleware
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
