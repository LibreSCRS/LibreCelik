# LibreCelik — Changelog

This file tracks user-visible and contributor-visible changes per
release. The canonical release notes live on GitHub Releases; this
file mirrors the highlights and is the local source of truth between
tags.

Format follows [Keep a Changelog](https://keepachangelog.com/en/1.1.0/);
LibreCelik versioning follows [Semantic Versioning](https://semver.org/).

## [Unreleased]

## [4.0.0-rc2] — 2026-05-08

### Added — internationalisation (i18n) audit infrastructure

- New static-analysis tool `tools/i18n_audit.py` (v1.0.0, 23 self-tests)
  scans `src/`, `plugins/`, `test/` for missing or broken
  `changeEvent(LanguageChange)` → `retranslateUi()` paths and for
  `qtTrId` callsites outside the `retranslateUi()` transitive
  closure. Six audit dimensions land in rc2: D1, D2, D3, D5, D6, D8.
  Inline allowlist mechanism with mandatory reason text:
  `// i18n-audit: ignore D<n>, <reason>`. Documented end-to-end in
  `tools/README-i18n-audit.md`.
- New runtime tests:
  - `test/i18n_settings_state_test.cpp` (3 cases) — D7 coverage
    for the b23a825 regression class (preference-state widgets must
    reflect the effective application state) and modal-dialog
    survivability across language switches.
  - `test/i18n_plugin_retranslate_test.cpp` (5 cases, parameterised)
    — D9 coverage for every production plugin card widget
    (EidWidget, HealthWidget, EMRTDWidget, EuVrcWidget, PIVWidget).
- New CI job `i18n-audit` on every PR runs the audit's pytest
  self-test plus the strict audit run; uploads JSON dump as artefact.
  Required check.
- New opt-in pre-commit hook `scripts/pre-commit-i18n.sh` for
  incremental local checks (`--diff-since=HEAD`).

### Fixed — runtime retranslation gaps

Closed every retranslation gap identified by the audit's baseline
sweep (67 findings). 22 are heuristic-correct flags on classes that
retranslate naturally via Qt's mechanisms (PDF print formatters,
item-view models via `Qt::DisplayRole`, modal dialogs re-opened
fresh after language switch, transient menus / file dialogs); these
carry inline allowlist annotations as a permanent record. The
remaining 45 are real user-visible retranslate gaps now fixed:

- LC widgets (rebuild-tier, April 2026 spec):
  `src/aboutdialog.cpp` (drop duplicated ctor `setWindowTitle`),
  `src/document/tokensection.{h,cpp}` (cache `CardFieldGroup` for
  rebuild path), `src/utils/securitystatuswidget.{h,cpp}` (cache
  `SecurityStatus`, extract `refreshSummaryRows` and
  `rebuildDetailRows` helpers), `src/settings/settingsdialog.cpp`
  (single source of truth in `retranslateUi`), `src/signing/
  fileselectionpage.{h,cpp}` and `src/signing/
  signatureplacementpage.{h,cpp}` (extract `retranslateUi` from
  inline `changeEvent` blocks).
- LC widget (rebuild-tier):
  `src/certificate/certificateviewerwidget.{h,cpp}` (add
  `changeEvent` + `retranslateUi`).
- Plugin widgets (rebuild-tier across `.so`):
  `plugins/{rs-eid,rs-health,emrtd,eu-vrc,piv}/*widget.{h,cpp}`
  — every plugin's `retranslateUi` now tears down the outer
  CollapsibleSection and rebuilds from cached `CardData::groups`
  (or, for `HealthWidget`, from a separate `rawGroups` cache so
  `transformPermanentlyValid`'s mutation does not block
  language switching).
- `plugins/emrtd/emrtdwidget`: dead methods `showAuthRequired` /
  `showError` removed (per `feedback_aggressive_legacy_removal.md`).

### Documented

- `CONTRIBUTING.md` adds a new "i18n discipline" section covering the
  static audit, runtime tests, contributor obligations on new
  `qtTrId` callsites (triple consistency, transitive reachability,
  canonical inline allowlist), and the locale-stable-wordlist
  amendment process.
- `tools/README-i18n-audit.md` documents the audit tool's usage,
  exit codes, allowlist syntax, and the canonical reason phrasings.

### Audit baseline numbers (rc2)

| Metric | Count |
|---|---|
| Total findings (baseline) | 67 |
| Allowlisted (heuristic-correct, no user-visible bug) | 22 |
| Fixed (real retranslate gaps closed) | 45 |
| Active findings post-rc2 | **0** |
| New runtime tests added | 8 (3 D7 + 5 D9) |
| LC widget files touched (rebuild remediation) | 6 |
| Plugin widget files touched | 5 (one per plugin) |
| LC tests passing | 115/115 (10 PIN-gated skips unrelated) |
