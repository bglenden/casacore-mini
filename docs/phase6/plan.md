# Phase 6 Plan (Detailed)

Date: 2026-02-23

## Objective

Move from isolated `AipsIO` primitives and synthetic tests to corpus-backed,
integration-level interoperability for keyword/record metadata and initial table
write bootstrap.

Immediate goals:

1. integrate binary keyword record decoding into active metadata extraction
   paths
2. implement composite `AipsIO` writing for `Record`/`TableRecord`-style
   objects with robust object-length handling
3. validate read/write behavior against corpus-backed casacore-produced binary
   fixtures (not only synthetic streams)
4. establish a minimal table directory write path for metadata-centric
   interoperability checks

## Documentation gate (carry-forward)

- Any Phase 6 task that introduces or changes public C++ API must include
  Doxygen updates in the same task.
- Phase closure requires passing CI docs generation with Doxygen warnings treated
  as errors.

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P6-W1` | API hardening for binary record I/O | Pending | explicit size/version guards, clear value-fidelity notes, and negative/malformed input handling coverage for `AipsIoWriter` + `read_aipsio_record` |
| `P6-W2` | Corpus-backed binary record fixtures | Pending | deterministic casacore-generated binary keyword/record fixtures + manifest/provenance updates |
| `P6-W3` | Binary keyword read integration | Pending | metadata extraction path that uses binary record decode where available, with deterministic fallback strategy |
| `P6-W4` | Composite `AipsIO` write support | Pending | writer support for composite record objects (`Record`/`TableRecord`-class payloads), including object-length patching |
| `P6-W5` | Table write bootstrap (metadata-first) | Pending | minimal metadata-focused table directory/files emission for controlled interop experiments |
| `P6-W6` | Phase 6 compatibility checks | Pending | `check_phase6` scripts and CI/local wiring validating corpus-backed binary read/write behavior |
| `P6-W7` | Phase 6 exit + Phase 7 prep | Pending | `docs/phase6/exit_report.md` and final prove-out candidate matrix for bidirectional artifact validation |

## Scope boundaries for Phase 6

In scope:

- binary keyword/record interoperability for persistence-critical metadata
- corpus-backed validation for new binary paths
- minimal metadata-first write bootstrap

Out of scope:

- full storage-manager payload write parity beyond scoped bootstrap targets
- optional ecosystem capabilities (UDF extras, Dysco, Python bindings)

## Phase 6 acceptance criteria

1. Binary keyword/record decoding is integrated into at least one active
   metadata extraction path used by compatibility checks.
2. Composite `AipsIO` write support exists for scoped record metadata objects
   with deterministic output and robust error handling.
3. Corpus-backed fixtures are used for both positive and malformed/interruption
   scenarios; synthetic-only evidence is no longer the sole basis for claims.
4. CI/local quality includes Phase 6 checks and enforces them by default.
5. Phase 6 exit report lists remaining gaps and a concrete candidate matrix for
   the final bidirectional artifact prove-out phase.
