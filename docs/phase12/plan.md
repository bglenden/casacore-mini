# Phase 12 Plan (Interop Carry-Forward + mdspan Modernization)

Date: 2026-02-28
Status: Pending (`Not Started`)

## Objective

Phase 12 has two mandatory outcomes:

1. close the active interoperability carry-forward regression on the
   Phase-8 coordinate artifacts (`coord-keywords`, `mixed-coords`) in
   `mini->casacore` matrix cells
2. introduce `mdspan`-based internal multidimensional array views/ownership
   adapters in the in-memory array model, while preserving external behavior
   and on-disk compatibility

This phase is compatibility-first: parity regressions are closed before broad
internal refactors.

## Entry condition

Phase 12 starts when all are true:

1. `bash tools/check_ci_build_lint_test_coverage.sh` passes on the branch
   baseline in an environment without casacore interop requirements
2. Phase-11 closeout artifacts are merged and status-consistent
3. a casacore-enabled host is available for Phase-8/Phase-12 matrix execution

## Definition of done

Phase 12 is complete only when all are true:

1. `tools/interop/run_phase8_matrix.sh` passes 24/24 cells on a
   casacore-enabled host
2. `coord-keywords: mini->casacore` and `mixed-coords: mini->casacore`
   both pass without manual overrides
3. coordinate semantic verifiers accept both supported schema layouts:
   - `spectralN.crval` / `spectralN.cdelt`
   - `spectralN.wcs.crval` / `spectralN.wcs.cdelt`
4. matrix failure diagnostics are actionable (failed cell, command pair,
   stderr, artifact path)
5. mdspan-backed array-view/owner abstractions are integrated into targeted
   modules with no external API break
6. full quality gates pass:
   - format
   - warnings-as-errors
   - clang-tidy
   - test suite
   - coverage threshold
7. docs (`plan`, `exit_report`, rolling plan) are status-consistent

## Scope boundaries

### In scope

1. Phase-8 coordinate interop regression closure (carry-forward)
2. interop matrix diagnostics hardening for faster failure triage
3. mdspan-based internal array model modernization in selected modules
4. migration adapters from existing `vector`-backed ownership to mdspan views
5. parity tests proving no behavior drift in migrated code paths

### Out of scope

1. changing public API contracts solely to expose mdspan types
2. storage-manager on-disk format changes
3. unrelated feature additions outside parity closure or array-model
   modernization

## Execution order and dependencies (strict)

Waves are sequential and fail-fast:

1. `P12-W1` coordinate interop regression closure (`mini->casacore`)
2. `P12-W2` interop matrix diagnostics hardening
3. `P12-W3` phase/gate wiring and documentation reconciliation
4. `P12-W4` mdspan foundation layer (view + ownership adapters)
5. `P12-W5` lattice/image internal migration tranche A
6. `P12-W6` table/selection/internal array migration tranche B
7. `P12-W7` hardening, stress, and mixed-path parity verification
8. `P12-W8` final closeout

No manual confirmation pauses between waves during normal execution.

## Required quality and documentation gates (every wave)

Mandatory commands:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. `bash tools/check_phase12_wX.sh` for current wave

Phase aggregate commands:

1. `bash tools/check_phase12.sh`
2. `bash tools/interop/run_phase8_matrix.sh <build_dir>` on casacore host

Gate rules:

1. warnings-as-errors and clang-tidy remain mandatory
2. required checks may not be downgraded to informational
3. parity-facing claims must be validated by executable checks
4. verifier crashes are automatic wave failures
5. command exit status is the pass/fail truth source

## Workstream table

| Wave | Status | Deliverables |
|---|---|---|
| `P12-W1` | Pending | semantic verifier fix for coordinate artifacts; matrix cells recover to pass |
| `P12-W2` | Pending | matrix diagnostics upgrade (no silent stderr loss; per-cell failure traces) |
| `P12-W3` | Pending | phase gate wiring (`check_phase12*`), docs/status reconciliation |
| `P12-W4` | Pending | mdspan foundation (`array_view`, owner adapters, compatibility shims) |
| `P12-W5` | Pending | migration tranche A (lattice/image internal paths) |
| `P12-W6` | Pending | migration tranche B (table/selection/internal multi-dim paths) |
| `P12-W7` | Pending | hardening and parity stress tests |
| `P12-W8` | Pending | closeout report, final gates, review packet completion |

## Per-wave implementation details

### `P12-W1` coordinate interop regression closure

Implementation tasks:

1. reproduce failures without hidden stderr:
   - `mini produce-coord-keywords` -> `casacore verify-coord-keywords`
   - `mini produce-mixed-coords` -> `casacore verify-mixed-coords`
2. update semantic verification in
   `tools/interop/casacore_interop_tool.cpp` to accept both spectral layouts:
   - flat fields: `spectralN.{crval,cdelt}`
   - nested fields: `spectralN.wcs.{crval,cdelt}`
3. keep strict numeric/semantic assertions unchanged outside field-location
   flexibility
4. prove 24/24 matrix recovery on casacore-enabled host

Expected touchpoints:

1. `tools/interop/casacore_interop_tool.cpp`
2. `tools/interop/run_phase8_matrix.sh` (only if needed for repro clarity)
3. `docs/phase8/known_differences.md` (if claims are stale)

Wave gate:

1. `bash tools/check_phase12_w1.sh <build_dir>` (must include real
   `mini->casacore` coordinate artifact checks)

### `P12-W2` matrix diagnostics hardening

Implementation tasks:

1. remove silent stderr suppression from matrix cell execution path
2. emit per-cell diagnostics:
   - artifact
   - direction (`producer->verifier`)
   - command pair used
   - artifact path
   - verifier stderr
3. keep final summary machine-parseable (`PASS`/`FAIL` counts)

Expected touchpoints:

1. `tools/interop/run_phase8_matrix.sh`
2. `docs/phase8/interop_artifact_inventory.md` (diagnostics contract note)

Wave gate:

1. `bash tools/check_phase12_w2.sh <build_dir>`

### `P12-W3` phase/gate wiring + docs reconciliation

Implementation tasks:

1. add `tools/check_phase12.sh` and `tools/check_phase12_w1.sh`..`w8.sh`
2. ensure `check_phase12_w1.sh` is casacore-aware:
   - fail if casacore is required and available but matrix regression remains
   - explicit skip only when casacore is unavailable
3. reconcile planning/status docs:
   - `docs/casacore_plan.md`
   - `docs/phase8/known_differences.md`
   - `docs/phase12/plan.md`

Expected touchpoints:

1. `tools/check_phase12.sh`
2. `tools/check_phase12_w*.sh`
3. docs listed above

Wave gate:

1. `bash tools/check_phase12_w3.sh <build_dir>`

### `P12-W4` mdspan foundation

Implementation tasks:

1. define internal mdspan facade compatible with current toolchains
2. define ownership adapters:
   - contiguous owner (`std::vector<T>`)
   - non-owning views
3. define shape/stride conversion utilities from existing shape types
4. add focused unit tests for view correctness and bounds assumptions

Expected touchpoints:

1. `include/casacore_mini/*array*` (new/updated internal headers)
2. `src/*array*` helper implementations
3. `tests/*array*` unit tests

Wave gate:

1. `bash tools/check_phase12_w4.sh <build_dir>`

### `P12-W5` migration tranche A (lattice/image)

Implementation tasks:

1. migrate selected lattice/image internal indexing paths to mdspan views
2. preserve external APIs and binary persistence behavior
3. add parity tests comparing pre/post migration outputs on same fixtures

Expected touchpoints:

1. `src/lattice*.cpp`
2. `src/image*.cpp`
3. `tests/lattice*_test.cpp`
4. `tests/image*_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w5.sh <build_dir>`

### `P12-W6` migration tranche B (table/selection internals)

Implementation tasks:

1. migrate additional internal multi-dimensional hot paths where safe
2. ensure no change to table access policy or storage-manager boundaries
3. add regression tests for shape and indexing semantics

Expected touchpoints:

1. selected `src/table*.cpp`, `src/ms_selection.cpp`, and helper internals
2. corresponding tests

Wave gate:

1. `bash tools/check_phase12_w6.sh <build_dir>`

### `P12-W7` hardening + stress

Implementation tasks:

1. run mixed producer/verifier stress passes for interop artifacts
2. run randomized indexing/property checks for mdspan migration paths
3. resolve all discovered correctness defects before closeout

Expected touchpoints:

1. `tools/interop/*`
2. `tests/*` hardening suites

Wave gate:

1. `bash tools/check_phase12_w7.sh <build_dir>`

### `P12-W8` final closeout

Implementation tasks:

1. rerun clean aggregate gates from fresh build dir
2. publish `docs/phase12/exit_report.md`
3. reconcile status across:
   - `docs/phase12/plan.md`
   - `docs/phase12/exit_report.md`
   - `docs/casacore_plan.md`
4. finalize per-wave review packets

Expected touchpoints:

1. `docs/phase12/exit_report.md`
2. `docs/phase12/review/P12-WX/*`
3. `docs/casacore_plan.md`

Wave gate:

1. `bash tools/check_phase12_w8.sh <build_dir>`

## Interoperability/evaluation contract

Required matrix evidence for closure:

1. all 24 cells from `tools/interop/run_phase8_matrix.sh` pass on casacore host
2. explicit logged evidence for both formerly failing cells:
   - `coord-keywords: mini->casacore`
   - `mixed-coords: mini->casacore`
3. if any semantic fallback path is used, emitted logs must identify which
   schema variant matched

## Review artifact contract (per wave)

Each completed wave must include:

1. `docs/phase12/review/P12-WX/summary.md`
2. `docs/phase12/review/P12-WX/files_changed.txt`
3. `docs/phase12/review/P12-WX/check_results.txt`
4. `docs/phase12/review/P12-WX/open_issues.md`
5. `docs/phase12/review/P12-WX/decisions.md`

Phase status file:

1. `docs/phase12/review/phase12_status.json`

## Immediate next step

Start `P12-W1`:

1. reproduce both failing `mini->casacore` coordinate cells with visible
   stderr
2. patch semantic verifier field-location handling
3. rerun full Phase-8 interop matrix on casacore-enabled host

## Autonomy policy

During Phase 12 execution, the implementing agent should proceed wave-by-wave
without waiting for user confirmation except when:

1. external credentials or privileged environment changes are required
2. a plan-level scope change is needed
3. destructive repository actions are requested

## Wave-gate design rules

1. no grep-only gates for functional claims
2. every functional gate executes real code paths
3. producer and verifier command failures are hard failures
4. segfaults or verifier crashes fail the wave immediately

## Closeout evidence protocol

1. final closeout must use a fresh build directory
2. command outputs in review packets must come from current branch state
3. plan/exit/rolling status text must match executable evidence

## Lint profile policy

1. lock active lint profile at Phase-12 kickoff
2. any lint-profile change requires:
   - explicit plan update
   - rationale
   - note in corresponding review packet decisions file
