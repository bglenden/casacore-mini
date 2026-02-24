# P7-W9 Summary: Full Tables Acceptance Suite

## Scope Implemented

Created an aggregate Phase 7 acceptance suite that runs all wave gates
(W3–W8) in sequence, and integrated it into the CI quality path.

1. **`tools/check_phase7.sh`** — Aggregate script that runs all six wave
   gates in order (W3→W4→W5→W6→W7→W8).
2. **`tools/check_phase7_w9.sh`** — Wave gate that validates the aggregate
   suite exists, passes, and is properly wired into the CI script.
3. **CI integration** — Replaced six individual `check_phase7_wN.sh` calls
   in `check_ci_build_lint_test_coverage.sh` with a single
   `check_phase7.sh` call.

## Public API Changes

None. All changes are in test infrastructure.

## Behavior Deltas

- `tools/check_ci_build_lint_test_coverage.sh` now calls `check_phase7.sh`
  instead of individual wave gate scripts.
- The W9 gate validates that the CI script does not reference individual
  wave checks, enforcing the aggregate pattern.
