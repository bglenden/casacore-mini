# Phase 9 Exit Report

Date: 2026-02-26
Status: Complete

## Objective

Deliver full MeasurementSet support in `casacore-mini` with compatibility-focused
behavior for lifecycle, schema, typed columns, selection, operations, and
cross-tool interoperability.

## Results Summary

| Metric | Result |
|--------|--------|
| Unit tests | 67/67 pass |
| Phase 9 wave gates | 14/14 pass |
| Interop matrix | 20/20 cells pass (5 artifacts x 4 directions) |
| Oracle conformance | `pass=330811 warn=0 fail=0` |
| Subtable schemas | 17 |
| Selection categories | 8/8 |
| Hardening suites | `ms_malformed_test` + oracle runtime gate |

## Wave Status

| Wave | Status | Deliverables |
|------|--------|-------------|
| W1 | Done | API surface map, contracts, tolerance policy, lint profile, gate scaffolding |
| W2 | Done | MeasurementSet core model: create/open/subtable wiring, ms_enums |
| W3 | Done | 17 subtable schemas: column enums, `make_*_desc()` factories, schema parity tests |
| W4 | Done | Typed column wrappers: `MsMainColumns` + required subtable column classes |
| W5 | Done | `MsWriter`: batch write, FK validation, subtable population |
| W6 | Done | Utility layer: `MsIter`, `StokesConverter`, `MsDopplerUtil`, `MsTileLayout`, `MsHistoryHandler` |
| W7 | Done | Selection foundation and parser behavior |
| W8 | Done | Full required selection-category behavior |
| W9 | Done | Read/introspection ops: `MsMetaData`, `MsSummary` |
| W10 | Done | Mutation ops: `MsConcat`, `MsFlagger` |
| W11 | Done | Interop produce/verify for 5 artifacts + malformed-input hardening |
| W12 | Done | Preliminary closeout docs/status alignment |
| W13 | Done | Oracle conformance gate on real upstream MS fixture |
| W14 | Done | Final closeout reruns, status reconciliation, and review packet completion |

## Interop Matrix Results

5 artifacts, 20 cells (5 artifacts x 4 directions):

| Artifact | mini→mini | mini→casacore | casacore→mini | casacore→casacore |
|----------|-----------|---------------|---------------|-------------------|
| ms-minimal | PASS | PASS | PASS | PASS |
| ms-representative | PASS | PASS | PASS | PASS |
| ms-optional-subtables | PASS | PASS | PASS | PASS |
| ms-concat | PASS | PASS | PASS | PASS |
| ms-selection-stress | PASS | PASS | PASS | PASS |

**20/20 cells pass.**

## W13/W14 Correctness Fixes

1. `TiledStMan` bool cell handling corrected to casacore-compatible bit-packed
   external layout (tile sizing, offsets, and read path).
2. `IncrementalStMan` header/index/value parsing corrected for robust
   endianness detection and bounds-safe decoding.
3. `Table::create` compatibility metadata corrected for casacore-openable
   subtables (`ColumnDesc` type tokens, fixed-shape options, manager type).
4. `MsWriter::flush_subtable` now preserves column/table keywords and existing
   table endianness when rebuilding subtables.
5. `SsmWriter` now emits valid empty `.f0i` files when indirect columns exist
   but no indirect payload rows have been written.
6. `Table::open`/`open_rw` now infer missing fixed array shapes for tiled
   columns from storage-manager metadata when shape is absent in `table.dat`.

## Closeout Evidence (Fresh Reruns)

1. `bash tools/check_phase9.sh build-p9-closeout` -> PASS
2. `bash tools/interop/run_phase9_matrix.sh build-p9-closeout` -> PASS (`20/20`)
3. `bash tools/check_ci_build_lint_test_coverage.sh build-p9-closeout-ci` -> PASS
   with `src/` line coverage `78.9%`

## Phase 10 Entry Conditions

Phase 10 may proceed because all entry conditions are satisfied:

1. `check_phase9` aggregate gate passes.
2. CI-style build/lint/test/coverage gate passes.
3. `plan` / `exit_report` / rolling plan statuses are reconciled.
4. Review packets are complete through `P9-W14`.

## Carry-Forward (Non-Blocking)

1. Optional subtable row-writer APIs (`DOPPLER`, `FREQ_OFFSET`, `SOURCE`,
   `SYSCAL`, `WEATHER`) are not yet populated by current MS artifact writers.
2. Advanced TaQL feature parity is out of current scope; Phase 9 selection
   coverage is constrained to the required categories.
3. Phase 8 carry-forward remains for full cross-frame
   `MFrequency`/`MRadialVelocity` conversion completeness.
4. Phase 11 still includes the explicit storage-manager fidelity audit for any
   remaining heuristic/simplification deltas versus upstream casacore.

## Known Differences

See `docs/phase9/known_differences.md` for current scoped differences.
