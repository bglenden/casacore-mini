# Phase 7 Plan (Full Tables, Implementation Spec)

Date: 2026-02-24

## Objective

Implement full Tables functionality in `casacore-mini` with bidirectional
interoperability against `casacore`, except for storage managers explicitly
deferred under the autonomy policy below.

## Definition of done

Phase 7 is complete only when:

1. `casacore-mini` supports full table metadata + data read/write for the Phase
   7 required storage-manager set.
2. Full table-directory artifacts produced by either system are consumable by
   both systems.
3. Semantic parity checks pass for schema, keywords, row/cell values, and array
   shape/value behavior across all 2x2 producer/consumer matrix cells.
4. Any deferred manager is explicitly documented with rationale and
   autonomy-policy decision records.

## Required storage-manager coverage

Must cover:

1. `StandardStMan`
2. `IncrementalStMan`
3. `TiledShapeStMan`
4. `TiledDataStMan`
5. `TiledColumnStMan`
6. `TiledCellStMan`

Deferral rules:

1. defer only with corpus-backed usage evidence
2. include interoperability risk statement
3. do not block phase execution waiting for user input; continue all remaining
   waves and document deferral in `docs/phase7/deferred_managers.md`
4. include a concrete carry-forward implementation plan for each deferred
   manager in `docs/phase7/exit_report.md`

## Baseline and dependencies

Already completed:

1. `P7-W1`: initial 2x2 matrix scaffold
2. `P7-W2`: corpus-backed record transcode cases

Carry-forward requirement from Phase 6:

1. binary record metadata extraction must be integrated into active
   `table_schema` path (not test-only)

## Execution order and dependencies

1. `P7-W3` -> `P7-W4` -> `P7-W5` -> `P7-W6` -> `P7-W7` -> `P7-W8` -> `P7-W9`
   -> `P7-W10`
2. No wave is marked done without passing wave-specific checks and review
   packet artifacts.

## Autonomy policy (no user input required during execution)

The implementing agent should complete all Phase 7 waves end-to-end without
pausing for user decisions.

Decision rules:

1. prefer implementation over deferral whenever technically feasible
2. if blocked on a design choice, apply the most conservative
   compatibility-preserving option and record it in the wave review packet
3. if an item must be deferred, document it and continue with subsequent waves
4. only request user input for actions outside ordinary implementation flow
   (for example publishing releases, destructive cleanup of user data, or
   changing agreed phase boundaries)

## Quality/documentation gate (every wave)

Mandatory pass:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. wave-specific Phase 7 checks

Mandatory docs:

1. Doxygen updates for changed public APIs in same wave
2. malformed-input tests for new binary parser/writer logic

## Workstreams (implementation-ready)

| ID | Status | Scope | Required deliverables |
|---|---|---|---|
| `P7-W1` | Done | Matrix scaffold | Existing |
| `P7-W2` | Done | Record fixture transcode matrix | Existing |
| `P7-W3` | Pending | Active binary metadata integration | Integrate binary record metadata extraction into `table_schema` active path; retain deterministic schema output; add regression tests for text-vs-binary parity. |
| `P7-W4` | Pending | Full `table.dat` body interoperability | Implement `table.dat` body parse/serialize support for `TableDesc`, column descriptors, and manager metadata records. |
| `P7-W5` | Pending | Table descriptor + keyword parity | Complete semantic parity for table/column descriptors and keyword records at table-directory scope. |
| `P7-W6` | Pending | Core manager interoperability | End-to-end read/write interoperability for `StandardStMan` and `IncrementalStMan` including row/cell operations. |
| `P7-W7` | Pending | Tiled manager interoperability | End-to-end interoperability for tiled manager set or explicit autonomy-policy deferrals. |
| `P7-W8` | Pending | Full directory matrix expansion | Expand matrix to full directory artifacts from both producers across required managers. |
| `P7-W9` | Pending | Full Tables acceptance suite | Deterministic full-Tables interop suite in local + CI. |
| `P7-W10` | Pending | Closeout | Exit report with evidence, deferred-manager register, and Phase 8 entry recommendation. |

## Wave details

### `P7-W3` Active binary metadata integration

Implementation tasks:

1. integrate binary metadata extraction into live schema path in
   `src/table_schema.cpp`
2. preserve existing external schema API behavior unless explicitly documented
3. ensure deterministic behavior for mixed text/binary fixture paths

Expected touchpoints:

1. `include/casacore_mini/table_schema.hpp`
2. `src/table_schema.cpp`
3. `src/table_schema_cli.cpp` (if schema output contract changes)
4. `tests/table_schema_test.cpp`
5. `tests/record_metadata_test.cpp` and/or new integration test(s)

Wave gate:

1. new `tools/check_phase7_w3.sh` wired into quality scripts
2. parity tests must fail on text-vs-binary extraction divergence

### `P7-W4` Full `table.dat` body interoperability

Implementation tasks:

1. extend beyond header-only handling to full scoped `table.dat` body
2. support parse/serialize for:
   - `TableDesc`
   - column descriptors
   - storage-manager metadata records
3. add malformed-input guards for body decoding paths

Expected touchpoints:

1. `include/casacore_mini/table_dat.hpp`
2. `src/table_dat.cpp`
3. `include/casacore_mini/table_dat_writer.hpp`
4. `src/table_dat_writer.cpp`
5. `tests/table_dat_test.cpp`
6. `tests/table_dat_writer_test.cpp`

Wave gate:

1. new `tools/check_phase7_w4.sh` validating body-level parity

### `P7-W5` Descriptor + keyword parity at full table scope

Implementation tasks:

1. close typed parity gaps for table/column descriptors
2. close typed parity gaps for keyword records at directory scope
3. document any unavoidable fidelity limitations

Expected touchpoints:

1. `include/casacore_mini/keyword_record.hpp`
2. `src/keyword_record.cpp`
3. `include/casacore_mini/record.hpp`
4. `src/record_io.cpp`
5. interop tools and comparator paths

Wave gate:

1. new `tools/check_phase7_w5.sh` validating descriptor/keyword semantic parity

### `P7-W6` Core manager interoperability (`StandardStMan`, `IncrementalStMan`)

Implementation tasks:

1. implement full read/write roundtrip for core manager table directories
2. validate append/read/update behavior for scalar and array columns
3. add cross-producer/cross-consumer directory matrix cases

Expected touchpoints:

1. table directory read/write modules under `src/`
2. interop tools in `src/interop_mini_tool.cpp` and
   `tools/interop/casacore_interop_tool.cpp`
3. matrix runner `tools/interop/run_phase7_matrix.sh`
4. new manager-focused tests

Wave gate:

1. new `tools/check_phase7_w6.sh` for core manager scenarios

### `P7-W7` Tiled manager interoperability

Implementation tasks:

1. implement tiled manager interoperability for:
   - `TiledShapeStMan`
   - `TiledDataStMan`
   - `TiledColumnStMan`
   - `TiledCellStMan`
2. if any manager is deferred, create explicit deferral record and continue
   with all remaining waves

Expected touchpoints:

1. tiled manager-specific serialization/deserialization modules
2. matrix tools and comparator fixtures
3. tests for tiled manager data and shape semantics

Wave gate:

1. new `tools/check_phase7_w7.sh`
2. if deferrals exist, `docs/phase7/deferred_managers.md` updated with
   rationale + carry-forward plan

### `P7-W8` Full directory matrix expansion

Implementation tasks:

1. matrix must run full directory artifacts (not single-file only)
2. include both producers and both consumers for every required manager case
3. preserve deterministic comparator outputs

Expected touchpoints:

1. `tools/interop/run_phase7_matrix.sh`
2. interop helper tooling and comparator scripts
3. CI wiring in `.github/workflows/quality.yml` and `tools/run_quality.sh`

Wave gate:

1. new `tools/check_phase7_w8.sh`

### `P7-W9` Full Tables acceptance suite

Implementation tasks:

1. aggregate all phase checks into a single deterministic acceptance suite
2. integrate acceptance suite into local and CI quality paths
3. enforce semantic parity gates for schema + keywords + data values + array
   behavior

Expected touchpoints:

1. `tools/check_phase7.sh` (aggregate)
2. `tools/run_quality.sh`
3. `.github/workflows/quality.yml`

Wave gate:

1. `bash tools/check_phase7.sh` passes locally and in CI

### `P7-W10` Phase closeout

Implementation tasks:

1. publish closeout report with evidence and residual gaps
2. include deferred-manager register (empty or explicit) with decision records
3. include Phase 8 kickoff risk transfer note

Required file:

1. `docs/phase7/exit_report.md`

## Mandatory review packet artifacts (for efficient AI review)

For each wave `P7-WX`, commit:

1. `docs/phase7/review/P7-WX/summary.md`
   - scope implemented
   - public API changes
   - behavior deltas
2. `docs/phase7/review/P7-WX/files_changed.txt`
   - one path per line
3. `docs/phase7/review/P7-WX/check_results.txt`
   - exact commands run + pass/fail
4. `docs/phase7/review/P7-WX/matrix_results.json`
   - machine-readable case list + outcome for each matrix cell
5. `docs/phase7/review/P7-WX/open_issues.md`
   - unresolved issues, deferrals, rationale
6. `docs/phase7/review/P7-WX/decisions.md`
   - autonomous decisions, assumptions, and tradeoffs made in the wave

Use template:

1. `docs/phase7/review_packet_template.md`

Review closure rule:

1. wave cannot be marked done without complete review packet

## Interop matrix contract (Phase 7)

Required for every in-scope artifact:

1. `casacore` -> `casacore`
2. `casacore` -> `casacore-mini`
3. `casacore-mini` -> `casacore`
4. `casacore-mini` -> `casacore-mini`

Pass/fail rules:

1. semantic interpretation parity is the oracle
2. canonical dumps are diagnostics only
3. no producer is compatible unless both consumers pass

## Immediate next step

Execute `P7-W3` using this plan and produce a complete `P7-W3` review packet.
