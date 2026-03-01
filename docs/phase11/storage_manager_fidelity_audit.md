# Storage Manager Fidelity Audit

Date: 2026-02-28 (P12-W12 re-audit)
Previous: Phase 11 audit (2026-02-27)

## Purpose

Audit all storage-manager reader+writer paths for behavioral simplifications
or heuristics compared to upstream casacore. Classify each finding as `Exact`
or `Simplified` with remediation plan.

## Methodology

1. Reviewed all SM reader/writer source in `src/` against upstream equivalents
   in `casacore-original/tables/DataMan/`
2. Cross-referenced with Phase 7 interop matrix results (all 24/24 cells pass)
3. Cross-referenced with Phase 10 interop matrix results (all 20/20 cells pass)
4. P12-W12 re-audit: verified create integration (P12-W2), mutation coverage
   (add_row/remove_row for all three SM families), and on-disk roundtrip fidelity

## Findings by Storage Manager

### StandardStMan (SSM)

| Component | Status | Evidence |
|-----------|--------|----------|
| SSM reader - bucket file parsing | Exact | Phase 7/10 interop; standard_stman_test |
| SSM reader - all scalar types | Exact | standard_stman_test |
| SSM reader - fixed-shape arrays | Exact | standard_stman_test |
| SSM reader - indirect (variable) arrays | Exact | standard_stman_test |
| SSM reader - string columns | Exact | standard_stman_test |
| SSM reader - endian handling | Exact | standard_stman_test |
| SSM writer - bucket management | Exact | table_test; demo_table_write |
| SSM writer - all scalar types | Exact | table_test |
| SSM writer - fixed-shape arrays | Exact | table_mutation_test (test_add_row_ssm_fixed_array) |
| SSM writer - indirect arrays | Exact | table_test |
| SSM create integration | Exact | Default in Table::create() |
| SSM add_row | Exact | table_mutation_test (test_add_row_ssm_scalar, test_add_row_ssm_fixed_array) |
| SSM remove_row | Exact | table_mutation_test (test_remove_row_ssm_scalar, test_remove_row_ssm_fixed_array) |

### IncrementalStMan (ISM)

| Component | Status | Evidence |
|-----------|--------|----------|
| ISM reader - bucket/index parsing | Exact | incremental_stman_test |
| ISM reader - all scalar types | Exact | incremental_stman_test |
| ISM reader - string columns | Exact | incremental_stman_test |
| ISM reader - endian handling | Exact | incremental_stman_test |
| ISM writer - bucket management | Exact | incremental_stman_test |
| ISM writer - all scalar types | Exact | incremental_stman_test |
| ISM create integration | Exact | TableCreateOptions{.sm_type = "IncrementalStMan"} |
| ISM add_row | Exact | table_mutation_test (test_add_row_ism) |
| ISM remove_row | Exact | table_mutation_test (test_remove_row_ism) |

### TiledColumnStMan

| Component | Status | Evidence |
|-----------|--------|----------|
| Reader - tile/hypercube parsing | Exact | Phase 10 interop; tiled_directory_test |
| Reader - all numeric types | Exact | Phase 10 interop (Bool bit-packing fixed) |
| Reader - endian handling | Exact | Phase 10 interop |
| Writer - tile allocation | Exact | tiled_directory_test |
| Create integration | Exact | TableCreateOptions{.sm_type = "TiledColumnStMan"} |
| add_row | Exact | table_mutation_test (test_add_row_tsm) |
| remove_row | Exact | table_mutation_test (test_remove_row_tsm) |

### TiledCellStMan

| Component | Status | Evidence |
|-----------|--------|----------|
| Reader - per-row hypercube | Exact | Phase 10 interop |
| Reader - all numeric types | Exact | Phase 10 interop |
| Writer - per-row tile allocation | Exact | tiled_directory_test |
| Create integration | Exact | TableCreateOptions{.sm_type = "TiledCellStMan"} |
| add_row / remove_row | Exact | Same code path as TiledColumnStMan (verified by test) |

### TiledShapeStMan

| Component | Status | Evidence |
|-----------|--------|----------|
| Reader - shape-grouped hypercubes | Exact | Phase 10 interop |
| Reader - all numeric types | Exact | Phase 10 interop |
| Writer - shape grouping | Exact | tiled_directory_test |
| Create integration | Exact | TableCreateOptions{.sm_type = "TiledShapeStMan"} |
| add_row / remove_row | Exact | Same code path as TiledColumnStMan |

### TiledDataStMan

| Component | Status | Evidence |
|-----------|--------|----------|
| Reader - coordinate-based hypercubes | Exact | Phase 10 interop |
| Reader - all numeric types | Exact | Phase 10 interop |
| Writer - coordinate tile allocation | Exact | tiled_directory_test |
| Create integration | Exact | TableCreateOptions{.sm_type = "TiledDataStMan"} |
| add_row / remove_row | Exact | Same code path as TiledColumnStMan |

## Summary

| Storage Manager | Reader | Writer | Create | add_row | remove_row |
|----------------|--------|--------|--------|---------|------------|
| StandardStMan | Exact | Exact | Exact | Exact | Exact |
| IncrementalStMan | Exact | Exact | Exact | Exact | Exact |
| TiledColumnStMan | Exact | Exact | Exact | Exact | Exact |
| TiledCellStMan | Exact | Exact | Exact | Exact | Exact |
| TiledShapeStMan | Exact | Exact | Exact | Exact | Exact |
| TiledDataStMan | Exact | Exact | Exact | Exact | Exact |

All six required storage managers are fidelity-exact across all operations:
create, open, read, write, add_row, and remove_row.

## P12-W12 fix: TSM add_row row-count ordering

During re-audit, discovered that `Table::add_row()` wrote the updated row
count to `table.dat` before re-opening TSM readers for data copy. This caused
`TSM cube coverage mismatch` errors when TSM readers tried to validate the old
hypercube against the new row count. Fixed by deferring the row count update
until after writer rebuild.

## Deferred-on-demand storage managers

See `docs/phase12/deferred_on_demand_policy.md` for non-core manager policy.
