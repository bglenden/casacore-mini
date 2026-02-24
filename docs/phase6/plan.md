# Phase 6 Plan (Detailed)

Date: 2026-02-23

## Objective

Move from isolated `AipsIO` primitives and synthetic tests to corpus-backed,
integration-level interoperability for keyword/record metadata and initial table
write bootstrap.

Immediate goals:

1. establish binary keyword record decoding in compatibility-facing extraction
   workflows
2. implement composite `AipsIO` writing for `Record`/`TableRecord`-style
   objects with robust object-length handling
3. validate read/write behavior against corpus-backed binary fixtures (not only
   synthetic streams), with explicit provenance of fixture source
4. establish a minimal `table.dat` header write bootstrap for metadata-centric
   interoperability checks

## Documentation gate (carry-forward)

- Any Phase 6 task that introduces or changes public C++ API must include
  Doxygen updates in the same task.
- Phase closure requires passing CI docs generation with Doxygen warnings treated
  as errors.

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P6-W1` | API hardening for binary record I/O | Done | explicit size/version guards, clear value-fidelity notes, and negative/malformed input handling coverage for `AipsIoWriter` + `read_aipsio_record` |
| `P6-W2` | Corpus-backed binary record fixtures | Done (scoped) | deterministic casacore-mini-generated binary keyword/record fixtures + provenance updates; casacore-produced binary fixtures deferred to Phase 7 |
| `P6-W3` | Binary keyword read integration | Done (scoped) | binary metadata extraction API validated by compatibility tests; live `table_schema` pipeline integration deferred |
| `P6-W4` | Composite `AipsIO` write support | Done | writer support for composite record objects (`Record`/`TableRecord`-class payloads), including object-length patching |
| `P6-W5` | Table write bootstrap (metadata-first) | Done (scoped) | header-only `table.dat` metadata write support for controlled interop experiments; full table directory/body emission deferred |
| `P6-W6` | Phase 6 compatibility checks | Done | `check_phase6` scripts and CI/local wiring validating corpus-backed binary read/write behavior |
| `P6-W7` | Phase 6 exit + Phase 7 prep | Done | `docs/phase6/exit_report.md` including deferred-item handoff and candidate matrix for bidirectional artifact validation |

## Phase 6 closure

Phase 6 closed 2026-02-23 for scoped bootstrap objectives. See
`docs/phase6/exit_report.md` for completed work, explicit deferrals, and the
Phase 7 prove-out candidate matrix.

## Scope boundaries for Phase 6

In scope:

- binary keyword/record interoperability for persistence-critical metadata
- corpus-backed validation for new binary paths
- minimal metadata-first write bootstrap

Out of scope:

- full storage-manager payload write parity beyond scoped bootstrap targets
- optional ecosystem capabilities (UDF extras, Dysco, Python bindings)

## Phase 6 acceptance criteria

1. Binary keyword/record decoding is integrated into at least one
   compatibility-facing metadata extraction workflow used by checks (dedicated
   binary-metadata tests in this phase).
2. Composite `AipsIO` write support exists for scoped record metadata objects
   with deterministic output and robust error handling.
3. Corpus-backed fixtures are used for positive interoperability checks, with
   malformed/interruption coverage retained in targeted unit tests; fixture
   source provenance is explicitly documented.
4. CI/local quality includes Phase 6 checks and enforces them by default.
5. Phase 6 exit report lists remaining gaps and a concrete candidate matrix for
   the final bidirectional artifact prove-out phase.
