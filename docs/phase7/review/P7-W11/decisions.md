# P7-W11 Decisions

## Autonomous decisions made

1. Made the matrix informational (not CI-gating) in `check_phase7.sh` since it
   will fail until W12–W16 implement SM data planes. The matrix becomes a CI
   gate in W17.
2. Reverted `check_phase7_w8.sh` to structural-only checks (grep for case
   names, run unit tests) rather than executing the full matrix, since W8 is a
   pre-recovery wave gate.
3. Used per-cell pass/fail tracking in `run_dir_case` rather than immediate
   exit-on-failure, to provide a complete matrix summary.

## Assumptions adopted

1. Record and TableDat cases (`run_case`) do not need the same per-cell
   tracking because they already pass all cells — kept existing verify_both
   behavior for these.

## Tradeoffs accepted

1. The W9 gate recursively invokes `check_phase7.sh` which now includes matrix
   execution. This creates a deep call chain that can hit resource limits in
   constrained environments. Acceptable because the W9 gate validates that the
   aggregate exists and has the right structure.
