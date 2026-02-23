# Phase 4 Plan (Kickoff)

Date: 2026-02-23

## Objective

Advance image/lattice metadata compatibility using typed keyword structures and
expanded extraction coverage while preparing transition paths away from
`showtableinfo` text dependency.

Immediate goals:

1. define and implement a typed nested keyword-record model for image/table
   metadata
2. broaden extraction of coordinate-related fields beyond the initial Phase 3
   subset
3. add fixture-backed automation for expanded image/lattice metadata checks

## Documentation gate (carry-forward)

- Any Phase 4 task that introduces or changes public C++ API must include
  Doxygen updates in the same task.
- Phase closure requires passing CI docs generation with Doxygen warnings treated
  as errors.

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P4-W1` | Typed keyword-record model v0 | In Progress | internal representation for nested keyword records with arrays/scalars and deterministic traversal |
| `P4-W2` | Image coordinate metadata expansion | Pending | extraction of additional `coords.direction0` and related image keyword fields with tests |
| `P4-W3` | Phase 4 compatibility checks | Pending | expanded fixture checks wired into CI/local quality for image/lattice metadata |
| `P4-W4` | Phase 4 exit recommendation | Pending | risk update and Phase 5 write-path preparation scope |

## `P4-W1` scope details

Planned now:

- define a compact internal keyword-value representation suitable for nested
  records and scalar/array payloads
- keep representation separate from legacy casacore naming and implementation
  internals
- start with read-only extraction from replayed `showtableinfo` fixtures

## Exit criteria for `P4-W1`

1. Typed keyword-record model is implemented with deterministic equality
   semantics.
2. At least one nested image keyword record path is parsed into the model.
3. Tests verify record structure and selected value extraction.
