# LibreCelik i18n audit tool

`tools/i18n_audit.py` is a static checker that finds runtime
language-switch (retranslate) gaps in LibreCelik's `qtTrId`-based
catalog. It is the rc2 enforcement layer on top of the April 2026
runtime retranslation design.

Authoritative documents:

- Spec: `knowledge/specs/2026-05-08-i18n-retranslate-audit-design.md`
- Plan: `knowledge/plans/2026-05-08-i18n-retranslate-audit-plan.md`
- Pattern reference: `knowledge/specs/archive/2026-04/2026-04-02-runtime-retranslation-design.md`

## Usage

```bash
# Run from inside LibreCelik/. Default: scan all dimensions, report all findings.
python3 tools/i18n_audit.py

# Strict mode for CI: exit 1 on first finding.
python3 tools/i18n_audit.py --strict

# Limit to specific dimensions.
python3 tools/i18n_audit.py --dims D1,D2,D3

# Machine-readable JSON output.
python3 tools/i18n_audit.py --json /tmp/audit.json

# Diff-scoped (audit only files changed since a git ref).
python3 tools/i18n_audit.py --diff-since HEAD

# Tool version.
python3 tools/i18n_audit.py --version
```

## Exit codes

| Code | Meaning |
|------|---------|
| 0    | No findings (after allowlist filtering) |
| 1    | At least one finding |
| 2    | Tool error (bad CLI, malformed allowlist, parse failure) |

## Audit dimensions

| Dim | Class of bug |
|-----|--------------|
| D1  | Class uses `qtTrId(...)` but lacks `retranslateUi()` or `changeEvent(LanguageChange)` |
| D2  | `retranslateUi()` exists but does not cover every same-class qtTrId callsite |
| D3  | `static QString s = qtTrId(...);` (snapshots first-call value forever) |
| D5  | qtTrId ID has no matching `<message id="...">` in en.ts / sr_RS.ts, or missing QT_TRID_NOOP |
| D6  | `.ts` orphan: `<message id="...">` for an ID that no source file uses |
| D8  | `setToolTip` / `setAccessibleName` / `setStatusTip` / `setWhatsThis` with `qtTrId` outside `retranslateUi` |

D4 (lambda capture of qtTrId result) and D7/D9 (preference state, plugin
widget) are covered separately — D4 by manual `git grep` in the
remediation flow; D7/D9 by gtest fixtures in `test/`.

## Allowlist

### Inline (preferred)

Append a same-line comment with mandatory reason text:

```cpp
static const QString kHelpUrl = qtTrId("lc-help-url");  // i18n-audit: ignore D3, locale-stable URL
```

Strict regex: `^// i18n-audit: ignore D\d+, .+$`. Reason text is
mandatory — a comment without a comma + non-empty reason is a parse
error (exit 2).

### File-level (escape hatch — discouraged)

`tools/i18n_audit.allowlist`:

```
src/legacy/oldwidget.cpp:D1:slated for removal in 4.1
```

Strict regex: `^[^:]+:D\d+:.+$`. PR review required for additions; lines
failing the pattern abort the tool with exit 2.

## Self-tests

```bash
pytest tools/test_i18n_audit.py -q
```

Required-green before any tool change ships. Determinism is part of
the contract: two consecutive runs on the same tree must produce
byte-identical JSON output.

## Versioning

Semver `__version__` declared at the top of `i18n_audit.py`.

- MAJOR — new dimension or stricter detection requiring allowlist format change.
- MINOR — opt-in feature added behind a flag.
- PATCH — heuristic refinement that cannot fail more code than before.

First release: `1.0.0`.
