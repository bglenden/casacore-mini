# Phase 5 Plan (Kickoff)

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
| `P5-W1` | Keyword-record fidelity contract v1 | Pending | explicit scalar type/array shape requirements for read+write parity and a staged API update plan |
| `P5-W2` | `AipsIO` write primitive slice | Pending | deterministic write functions for selected primitive/object-header paths + round-trip tests |
| `P5-W3` | Direct keyword binary read bridge | Pending | initial extraction path from table keyword binary structures to normalized typed model |
| `P5-W4` | Write-path metadata compatibility checks | Pending | fixture-backed checks and comparator hooks for write/read metadata parity |
| `P5-W5` | Phase 5 exit recommendation | Pending | residual write-path risks and prioritized scope for broader table/MS/image write support |

## Phase 5 sequencing notes

1. Complete `P5-W1` before non-trivial write API expansion so typed fidelity and
   shape semantics are locked early.
2. Keep initial write scope narrow and compatibility-first:
   - primitive values
   - object headers
   - metadata/keyword records
3. Delay broad storage-manager payload writes until metadata round-trip behavior
   is proven against fixtures and comparators.
