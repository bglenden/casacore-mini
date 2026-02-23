# Phase 5 Plan (Closed)

Date: 2026-02-23

## Objective

Prepare and begin compatibility-focused write-path work while reducing remaining
text-source dependencies in keyword/table metadata handling.

Immediate goals:

1. define the minimal type/shape fidelity required for write-safe keyword
   records
2. add deterministic binary write primitives that round-trip with current read
   paths
3. establish first write-path compatibility checks for metadata surfaces before
   broad table payload writing

## Documentation gate (carry-forward)

- Any Phase 5 task that introduces or changes public C++ API must include
  Doxygen updates in the same task.
- Phase closure requires passing CI docs generation with Doxygen warnings treated
  as errors.

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P5-W1` | Keyword-record fidelity contract v1 | Done | `docs/phase5/keyword_record_fidelity_v1.md` |
| `P5-W2` | `AipsIO` write primitive slice | Done | `aipsio_writer.hpp/cpp` + `aipsio_writer_test.cpp` |
| `P5-W3` | Direct keyword binary read bridge | Done | `aipsio_record_reader.hpp/cpp` + `aipsio_record_reader_test.cpp` |
| `P5-W4` | Write-path metadata compatibility checks | Done | `tools/check_phase5.sh` wired into `run_quality.sh` and CI |
| `P5-W5` | Phase 5 exit recommendation | Done | `docs/phase5/exit_report.md` |

## Closure status

Phase 5 is closed. Completion details and residual risks are captured in:

- `docs/phase5/exit_report.md`

Phase 6 detailed execution now continues in:

- `docs/phase6/plan.md`

## Phase 5 sequencing notes

1. Complete `P5-W1` before non-trivial write API expansion so typed fidelity and
   shape semantics are locked early.
2. Keep initial write scope narrow and compatibility-first:
   - primitive values
   - object headers
   - metadata/keyword records
3. Delay broad storage-manager payload writes until metadata round-trip behavior
   is proven against fixtures and comparators.
