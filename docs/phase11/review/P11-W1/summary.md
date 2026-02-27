# Phase 11 Wave 1 Review: Remaining-Capabilities Audit

Date: 2026-02-27
Wave: P11-W1
Commit range: (first P11 commit)

## Scope and intent

1. Complete remaining-capabilities audit of all upstream casacore surfaces
   not implemented in Phases 1-10.
2. Assign explicit Include/Exclude dispositions with rationale.
3. Produce TaQL and MSSelection feature checklists for implementation tracking.
4. Audit storage-manager reader/writer fidelity.

## Audit summary

- 32 capability rows audited across 8 upstream modules
- 13 capabilities marked Include (all TaQL, MSSelection, SM integration)
- 15 capabilities marked Exclude (scimath, scimath_f, fits, msfits, derivedmscal)
- 4 existing capabilities confirmed Exact

### TaQL checklist: 115 rows covering all 10 command families
### MSSelection checklist: 93 rows covering all 12 categories
### API surface map: 42 entries for new/existing public symbols

## Storage manager fidelity

All 6 required SM readers and writers classified as Exact.
Only simplification: ISM and TSM writers not yet integrated into
Table::create() - remediation assigned to P11-W4.

## Gate results

```
P11-W1: pass=17 fail=0 GATE PASSED
```
