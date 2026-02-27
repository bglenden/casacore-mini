# Phase 11 Review Packet Template (`P11-WX`)

Date:
Wave: `P11-WX`
Owner:
Commit range:

## 1. Scope and intent

1. Wave objective.
2. Capabilities targeted in this wave.
3. Explicit non-goals for this wave.

## 2. Checklist progress

1. Source checklist file(s):
   - `docs/phase11/taql_command_checklist.csv`
   - `docs/phase11/msselection_capability_checklist.csv`
2. Row summary by status (`Pending`, `InProgress`, `Done`, `Blocked`).
3. Rows completed in this wave (IDs/feature names).
4. Rows deferred/blocked and rationale.

## 3. Code and API evidence

1. Public API symbols added/changed.
2. Internal implementation touchpoints.
3. Doxygen/API map updates:
   - `docs/phase11/api_surface_map.csv`
4. Commits and PR links associated with completed rows.

## 4. Test and gate evidence

1. Commands run and pass/fail:
   - `bash tools/check_phase11_wX.sh`
   - `bash tools/check_ci_build_lint_test_coverage.sh`
   - additional wave-specific commands
2. Evidence artifacts (exact repository paths).
3. Malformed/negative tests added.
4. Regressions fixed in this wave.

## 5. Oracle and interop evidence

1. Oracle artifacts produced/updated (exact paths).
2. Oracle diffs (exact paths, or `none` with reason).
3. Matrix cells exercised and outcomes.
4. Cross-toolchain parity notes.

## 6. Open issues and risk

1. Open blockers with owner and next action.
2. Known risks carried to next wave.
3. Follow-up tasks.

## 7. Status file sync

1. Confirm `docs/phase11/review/phase11_status.json` was updated by wave gate scripts.
2. Confirm status file `wave`, `gate_results`, and `new_evidence_paths` match this packet.
