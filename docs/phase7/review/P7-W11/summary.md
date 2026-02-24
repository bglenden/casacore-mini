# P7-W11 Summary

## Scope implemented

Strict interop gating and matrix truthfulness. The interop matrix runner
(`run_phase7_matrix.sh`) now reports all four matrix cells per directory case
with strict pass/fail tracking and exits non-zero on any failure. The previous
non-fatal bypass for mini→casacore verification has been removed.

## Public API changes

None. This wave modifies only shell scripts and documentation.

## Behavioral changes

1. `run_phase7_matrix.sh`: `run_dir_case()` now tests all four matrix cells
   independently (casacore→casacore, casacore→mini, mini→mini, mini→casacore)
   with pass/fail counters. Exits non-zero if any cell fails.
2. `check_phase7.sh`: Now includes W9 and W11 gates. Runs the full interop
   matrix when casacore is available (informational, not CI-gating until W17).
3. `check_phase7_w8.sh`: Reverted to structural checks only (no matrix
   execution — that is the aggregate's responsibility).
4. New `check_phase7_w11.sh`: Verifies the matrix script has strict behavior,
   all four cell labels, pass/fail counters, and that the aggregate references
   the matrix runner.

## Deferred items

Matrix failures for mini→casacore (6 of 24 directory cells) are expected and
will be resolved by W12–W16 as storage manager data planes are implemented.
