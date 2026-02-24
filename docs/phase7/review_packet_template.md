# Phase 7 Review Packet Template

Use this template for each completed wave by creating:

1. `docs/phase7/review/P7-WX/summary.md`
2. `docs/phase7/review/P7-WX/files_changed.txt`
3. `docs/phase7/review/P7-WX/check_results.txt`
4. `docs/phase7/review/P7-WX/matrix_results.json`
5. `docs/phase7/review/P7-WX/open_issues.md`
6. `docs/phase7/review/P7-WX/decisions.md`

## `summary.md` template

```md
# P7-WX Summary

## Scope implemented

## Public API changes

## Behavioral changes

## Deferred items (if any)
```

## `files_changed.txt` template

One repository-relative file path per line.

## `check_results.txt` template

Include exact commands and outcomes, for example:

```text
bash tools/check_format.sh -> PASS
bash tools/check_ci_build_lint_test_coverage.sh -> PASS
bash tools/check_docs.sh -> PASS
bash tools/check_phase7_wX.sh -> PASS
```

## `matrix_results.json` template

Use machine-readable results, for example:

```json
{
  "wave": "P7-WX",
  "cases": [
    {
      "artifact": "example_artifact",
      "matrix": {
        "casacore_to_casacore": "PASS",
        "casacore_to_mini": "PASS",
        "mini_to_casacore": "PASS",
        "mini_to_mini": "PASS"
      }
    }
  ]
}
```

## `open_issues.md` template

```md
# P7-WX Open Issues

## Blocking issues

## Non-blocking issues

## Proposed follow-ups
```

## `decisions.md` template

```md
# P7-WX Decisions

## Autonomous decisions made

## Assumptions adopted

## Tradeoffs accepted
```
