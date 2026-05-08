# i18n retranslate audit — Phase B.3 escalation report

**Status:** Gate triggered — awaiting caller decision before B.4 remediation.
**Date:** 2026-05-08
**Branch:** `feat/i18n-retranslate-audit-rc2`
**Tool:** `tools/i18n_audit.py` v1.0.0 (23 pytest cases green, deterministic)
**Baseline:** `tools/audit-baseline.json` (committed in this worktree).

## 1. Trigger

Per plan B.3 thresholds: **N > 30 total** OR **D1+D2 > 15** OR
**any single widget > 5 findings**.

Audit run reports:

| Dim | Severity | Count |
|-----|----------|-------|
| D1  | high     | 18    |
| D2  | high     | 44    |
| D3  | high     | 0     |
| D5  | medium   | 0     |
| D6  | low      | 0     |
| D8  | medium   | 5     |
| **Total** |    | **67** |

D4 manual grep produced 6 callsites; all are immediate-use within the
same function (no lambda capture, no member storage). **D4 = 0 bugs.**

All three thresholds tripped:
- 67 > 30 (total),
- 18 + 44 = 62 > 15 (rebuild-tier load),
- single widgets above 5: EuVrcWidget=10, PIVWidget=7, HealthWidget=7.

## 2. Findings classified by April 2026 retranslate spec

The April spec partitions remediation into **simple** (re-apply
`setText` in `retranslateUi`), **rebuild** (clear and repopulate item
trees / lists / dynamic groups), **plugin** (rebuild across `.so`).

### 2.1 Plugin widgets (rebuild-tier, plugin scope) — 34 findings

EuVrcWidget=10, PIVWidget=7, HealthWidget=7, EMRTDWidget=5, EidWidget=5
(all D2). Each plugin widget's `retranslateUi` is stub-tier (only outer
section title + print button tooltip) while progressive `addGroup`,
`buildShell`, `buildHolderSection`, etc. populate dozens of qtTrId-derived
labels outside the closure. Per April spec §"Plugin widgets — rebuild
needed": each must clear dynamic content on `LanguageChange` and rebuild
from cached `CardData`.

### 2.2 LC application widgets (rebuild-tier, app scope) — 13 findings

| Widget | D1 | D2 | D8 | Notes |
|--------|----|----|----|-------|
| TokenSection             | 0 | 3 | 1 | tree rebuild + tooltip in ctor. |
| SettingsDialog           | 0 | 3 | 0 | combo items, TL/TSA add dialogs. |
| SecurityStatusWidget     | 0 | 3 | 0 | section title + status row labels. |
| FileSelectionPage        | 0 | 0 | 2 | 2× tooltip outside `retranslateUi` (D8). |
| SignaturePlacementPage   | 0 | 0 | 2 | same pattern. |
| AboutDialog              | 0 | 1 | 0 | `loadLicense()` builds plain-text license body. |

### 2.3 Plugin TextDocument + WidgetPlugin classes — 10 findings (D1)

`{EMRTD,EuVrc,PIV,EId,Health}TextDocument` are PDF print formatters
(fresh `QTextDocument` per print job; language switch retranslates
naturally on next print). `{EMRTD,EuVrc,PIV,RsEid,RsHealth}WidgetPlugin`
are `QObject` plugin entry points whose `displayName / description`
are read once by `CardWidgetPluginRegistry` at plugin discovery.
**Both groups: candidates for inline allowlist** (no runtime
user-visible retranslate gap).

### 2.4 LC model + dialog classes — 8 findings (D1)

- `Certificate{Hierarchy,Properties,TreeView}Model` — `data()` called
  per redraw, naturally retranslates (Qt::DisplayRole). **Allowlist.**
- `CertificateViewerDlg`, `ChangePinDlg` — modal dialogs re-opened
  fresh after language switch. **Allowlist.**
- `TextDocument` (print formatter, src/document/printing). **Allowlist.**
- `CertificateViewerWidget` — rebuild-tier needed for live retranslate.
- `AsyncCardReader` — qtTrId in error-emit lambda; **simple-tier fix**.

## 3. Recommendation

### 3.1 Defer with allowlist (4.1 backlog) — ~17 findings

PDF print formatters, plugin metadata readers, item-view models, and
modal dialogs do not present a user-visible retranslate bug — Qt
retranslates naturally via `data()` re-invocation or fresh widget
construction. Canonical inline annotations:

```cpp
// i18n-audit: ignore D1, PDF print formatter — fresh QTextDocument per print run
// i18n-audit: ignore D1, plugin metadata read once at registry load
// i18n-audit: ignore D1, item-view model retranslates via Qt::DisplayRole on next paint
// i18n-audit: ignore D1, modal dialog re-opened fresh after language switch
```

### 3.2 Fix for rc2 — ~50 findings

Plugin widgets (34) plus TokenSection / SettingsDialog /
SecurityStatusWidget / FileSelectionPage / SignaturePlacementPage /
AboutDialog (13) plus AsyncCardReader / CertificateViewerWidget (2)
are real user-facing retranslate gaps. The April 2026 spec enumerates
the rebuild pattern for each plugin widget; LC widgets need the same
recipe applied consistently. Estimated effort: ~8h engineering total
(5h plugin widgets, 3h LC widgets). Reachable inside an rc2 sprint
but not in a single session.

### 3.3 Risk

The allowlist-deferred subset delivers most of the user-visible value
at lower cost; the allowlist annotations remain a permanent record
that triggers PR review when those classes are next modified.

## 4. Stop point

Per task instructions, no remediation commits land before caller
acknowledges. The following artefacts are committed in this worktree:

- `tools/audit-baseline.json` — full JSON dump (67 findings).
- `tools/audit-baseline-report.md` — this report.
- `tools/i18n_audit.py` v1.0.0, `tools/test_i18n_audit.py` (23 tests),
  `tools/i18n_audit.allowlist` (empty),
  `tools/README-i18n-audit.md`.
- `test/i18n_test_support/` (fixture, mocks, wordlist).
- `test/i18n_test_support_self_test.cpp` (7 fixture self-tests).
- `test/CMakeLists.txt` extension.

**Awaiting caller decision: proceed with full remediation (path A,
~8h), or proceed with allowlist-deferred subset (path B,
~5h plus ~17 allowlist entries deferred to 4.1 backlog).**
