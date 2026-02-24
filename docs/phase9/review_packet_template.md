# Phase 9 Review Packet Template

Use this template for each completed wave by creating:

1. `docs/phase9/review/P9-WX/summary.md`
2. `docs/phase9/review/P9-WX/files_changed.txt`
3. `docs/phase9/review/P9-WX/check_results.txt`
4. `docs/phase9/review/P9-WX/matrix_results.json`
5. `docs/phase9/review/P9-WX/open_issues.md`
6. `docs/phase9/review/P9-WX/decisions.md`

## `summary.md` template

```md
# P9-WX Summary

## Scope implemented

## Public API changes

## Behavioral changes

## Interop coverage added

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
bash tools/check_phase9_wX.sh -> PASS
```

## `matrix_results.json` template

Use machine-readable results:

```json
{
  "wave": "P9-WX",
  "cases": [
    {
      "artifact": "example_ms_artifact",
      "semantics": {
        "schema_keywords": "PASS",
        "subtable_links": "PASS",
        "cell_values": "PASS",
        "selection_rowsets": "PASS",
        "operation_outputs": "PASS"
      },
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
# P9-WX Open Issues

## Blocking issues

## Non-blocking issues

## Proposed follow-ups
```

## `decisions.md` template

```md
# P9-WX Decisions

## Autonomous decisions made

## Assumptions adopted

## Tradeoffs accepted
```
