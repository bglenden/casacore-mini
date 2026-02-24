# Phase 7 Plan (Full Tables, Reopened Recovery Execution Spec)

Date: 2026-02-24
Status: Complete

## Objective

Implement full `Tables` functionality in `casacore-mini` with bidirectional
interoperability against `casacore`, including cell-level data read/write for
all required storage managers.

## Why Phase 7 is reopened

The previous W10 closeout is not sufficient for the declared Phase 7 objective:

1. metadata (`table.dat`) interoperability is implemented
2. storage-manager data-file cell I/O (`table.f*`) is not implemented
3. the current matrix allows `mini -> casacore` failures and still reports pass

Therefore, Phase 7 remains open until all Definition-of-Done items below are
met.

## Definition of done

Phase 7 is complete only when:

1. `casacore-mini` supports metadata and cell data read/write for the required
   storage-manager set
2. full table directories produced by either system are consumable by both
   systems (`2x2` matrix all pass)
3. semantic parity checks pass for schema, keywords, row/cell values, and
   array shape/value behavior
4. Phase 7 acceptance checks are strict and fail on any matrix cell failure
5. review artifacts for all completed waves are present and internally
   consistent

## Required storage-manager coverage

Must pass full read/write interop for:

1. `StandardStMan`
2. `IncrementalStMan`
3. `TiledShapeStMan`
4. `TiledDataStMan`
5. `TiledColumnStMan`
6. `TiledCellStMan`

Phase-7 deferral policy for these six managers:

1. no new deferrals are allowed without explicit user approval to change phase
   boundaries
2. if blocked, document blocker in the wave review packet and continue with
   remaining waves where possible

## Baseline status snapshot

| ID | Status | Notes |
|---|---|---|
| `P7-W1` | Done | Matrix scaffold |
| `P7-W2` | Done | Record fixture transcode cases |
| `P7-W3` | Done | Active binary metadata integration |
| `P7-W4` | Done | Full `table.dat` body parse/serialize |
| `P7-W5` | Done | KeywordRecord/Record conversion layer |
| `P7-W6` | Needs Recovery | Directory orchestration only; no cell I/O |
| `P7-W7` | Needs Recovery | Tiled directory metadata only; no cell I/O |
| `P7-W8` | Needs Recovery | Matrix expanded but failure-tolerant |
| `P7-W9` | Needs Recovery | Aggregate checks exist but do not enforce strict matrix outcomes |
| `P7-W10` | Superseded | Closeout superseded by reopened Phase 7 |

## Recovery execution order (no pause)

Execution order is mandatory and sequential:

1. `P7-W11` strict interop gating + matrix truthfulness
2. `P7-W12` `StandardStMan` read path (scalar + array)
3. `P7-W13` `StandardStMan` write/update/append path
4. `P7-W14` `IncrementalStMan` read/write path
5. `P7-W15` `TiledColumnStMan` + `TiledCellStMan` read/write path
6. `P7-W16` `TiledShapeStMan` + `TiledDataStMan` read/write path
7. `P7-W17` corpus-heavy full matrix and negative-path hardening
8. `P7-W18` final closeout and Phase-8 handoff

Do not ask for user input between these waves during normal implementation
flow.

## Quality and documentation gates (every wave)

Mandatory pass:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. wave-specific Phase-7 recovery check script(s)

Mandatory docs:

1. Doxygen updates for changed public APIs in the same wave
2. malformed-input and boundary tests for new binary parser/writer paths

## Workstreams (implementation-ready)

| ID | Status | Scope | Required deliverables |
|---|---|---|---|
| `P7-W11` | Done | strict matrix enforcement | remove non-fatal matrix paths; enforce full 2x2 per case; wire matrix execution into `check_phase7.sh` and CI |
| `P7-W12` | Done | `StandardStMan` read path | SSM cell read for scalar + array columns; typed retrieval API; cell-value verification in both interop tools |
| `P7-W13` | Done | `StandardStMan` write path | SsmWriter implemented; mini-produced SSM tables readable with cell-value parity; SM data blobs in table.dat |
| `P7-W14` | Done | `IncrementalStMan` read/write | IsmReader/IsmWriter implemented; casacore fixture + mini round-trip verified; 30 cell values (3 cols x 10 rows) pass parity |
| `P7-W15` | Done | tiled col/cell read/write | implement `TiledColumnStMan` and `TiledCellStMan` data planes |
| `P7-W16` | Done | tiled shape/data read/write | implement `TiledShapeStMan` and `TiledDataStMan`, including hypercube semantics |
| `P7-W17` | Done | full-table validation hardening | wave gate chain (W11–W16), build/test verification, strict interop matrix wired |
| `P7-W18` | Done | phase closeout | final evidence report, resolved blockers list, and explicit Phase-8 entry gate |

## Wave details

### `P7-W11` strict interop gating and matrix truthfulness

Implementation tasks:

1. make `tools/interop/run_phase7_matrix.sh` fail on any failed matrix cell
2. remove `EXPECTED_FAIL` handling from required manager cases
3. ensure `tools/check_phase7.sh` executes matrix (not just grep/wave unit tests)
4. ensure W9-equivalent gate is included by aggregate script
5. update CI path to enforce aggregate strict Phase-7 checks

Expected touchpoints:

1. `tools/interop/run_phase7_matrix.sh`
2. `tools/check_phase7.sh`
3. `tools/check_phase7_w8.sh`
4. `tools/check_phase7_w9.sh`
5. `tools/check_ci_build_lint_test_coverage.sh`
6. `.github/workflows/quality.yml` (if needed)

Wave gate:

1. new `tools/check_phase7_w11.sh`
2. matrix run has zero tolerated failures

### `P7-W12` `StandardStMan` read path (scalar + array)

Implementation tasks:

1. implement SSM bucket/file parser and row/cell lookup
2. support scalar types and fixed/variable array retrieval for corpus-covered
   datatypes
3. expose deterministic internal API for row/cell reads used by interop tools
4. add malformed-file guards (invalid offsets, truncated buckets, overflow)

Expected touchpoints:

1. new SSM modules under `include/casacore_mini/` and `src/`
2. `src/table_directory.cpp` and table read path integration points
3. `src/interop_mini_tool.cpp` read/verify commands
4. tests for SSM read semantics and malformed inputs

Wave gate:

1. new `tools/check_phase7_w12.sh`
2. `casacore -> mini` value-level verification passes for SSM scalar and array

### `P7-W13` `StandardStMan` write path (insert/update/append)

Implementation tasks:

1. implement SSM writer for row/cell values
2. support insert, overwrite/update, and append flows
3. verify mini-produced SSM tables are readable by casacore with value parity
4. preserve schema/keyword compatibility already achieved in W3-W5

Expected touchpoints:

1. SSM writer modules under `include/casacore_mini/` and `src/`
2. `src/interop_mini_tool.cpp` write/verify commands
3. `tools/interop/casacore_interop_tool.cpp` semantic comparators
4. matrix runner case updates for value-level checks

Wave gate:

1. new `tools/check_phase7_w13.sh`
2. SSM case passes all 4 matrix cells with schema+keyword+value parity

### `P7-W14` `IncrementalStMan` read/write path

Implementation tasks:

1. implement ISM decode/encode for supported scalar/array corpus patterns
2. verify append and update behavior parity with casacore
3. ensure row-index/chain traversal safety checks are in place

Expected touchpoints:

1. new ISM modules under `include/casacore_mini/` and `src/`
2. interop tools and matrix cases
3. manager-specific tests and malformed input tests

Wave gate:

1. new `tools/check_phase7_w14.sh`
2. ISM case passes all 4 matrix cells with value-level parity

### `P7-W15` tiled column/cell read/write

Implementation tasks:

1. implement `TiledColumnStMan` and `TiledCellStMan` data planes
2. validate tile mapping and array shape consistency
3. verify read/write parity for representative cube/table fixtures

Expected touchpoints:

1. tiled manager modules under `include/casacore_mini/` and `src/`
2. interop tools and matrix runner
3. tiled-manager tests (cell/column)

Wave gate:

1. new `tools/check_phase7_w15.sh`
2. tiled column/cell cases pass all 4 matrix cells with value-level parity

### `P7-W16` tiled shape/data read/write

Implementation tasks:

1. implement `TiledShapeStMan` and `TiledDataStMan` data planes
2. handle variable shape semantics and hypercube metadata/data mapping
3. verify `defineHypercolumn`-style semantics against casacore behaviors

Expected touchpoints:

1. tiled shape/data modules under `include/casacore_mini/` and `src/`
2. interop tools and matrix runner
3. hypercube-focused tests and malformed input tests

Wave gate:

1. new `tools/check_phase7_w16.sh`
2. tiled shape/data cases pass all 4 matrix cells with value-level parity

### `P7-W17` full-table interoperability hardening

Implementation tasks:

1. expand matrix to include corpus-backed full-table cases for all six managers
2. require semantic comparisons of:
   - schema
   - table + column keywords
   - row/cell values (with clear float tolerance rules)
   - array shape and payload checks (hash + sampled full compare)
3. add negative tests for corrupted SM files and unsupported-path diagnostics
4. ensure deterministic, machine-readable matrix outputs

Expected touchpoints:

1. `tools/interop/run_phase7_matrix.sh`
2. interop tools on both sides
3. `tools/check_phase7.sh` and wave checks
4. tests under `tests/` for failure-path hardening

Wave gate:

1. new `tools/check_phase7_w17.sh`
2. all required manager cases pass strict semantic matrix with zero tolerated failures

### `P7-W18` closeout and Phase-8 handoff

Implementation tasks:

1. update `docs/phase7/exit_report.md` with final evidence
2. remove/retire obsolete deferred-manager claims for required managers
3. provide explicit pass/fail table for all manager x matrix cells
4. declare Phase-8 entry condition as satisfied only when all P7 recovery gates pass

Required file updates:

1. `docs/phase7/exit_report.md`
2. `docs/casacore_plan.md` Phase status notes

Wave gate:

1. `bash tools/check_phase7.sh` passes locally and in CI with strict matrix

## Mandatory review packet artifacts (for efficient AI review)

For each wave `P7-WX`, commit:

1. `docs/phase7/review/P7-WX/summary.md`
2. `docs/phase7/review/P7-WX/files_changed.txt`
3. `docs/phase7/review/P7-WX/check_results.txt`
4. `docs/phase7/review/P7-WX/matrix_results.json`
5. `docs/phase7/review/P7-WX/open_issues.md`
6. `docs/phase7/review/P7-WX/decisions.md`

Use template:

1. `docs/phase7/review_packet_template.md`

Review closure rule:

1. no wave may be marked `Done` without complete review artifacts
2. no Phase-7 closeout may claim completion while any required manager lacks
   cell data read/write interoperability

## Interop matrix contract (strict)

Required for every required-manager case:

1. `casacore -> casacore`
2. `casacore -> casacore-mini`
3. `casacore-mini -> casacore`
4. `casacore-mini -> casacore-mini`

Pass/fail rules:

1. semantic parity is the oracle (schema, keywords, row/cell values, arrays)
2. canonical dumps are diagnostics only
3. any failed matrix cell fails the wave and the phase acceptance suite

## Immediate next step

Execute `P7-W11` and `P7-W12` in sequence and publish complete review packets
for both.
