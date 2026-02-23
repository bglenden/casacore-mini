# Phase 2 Plan (Kickoff)

Date: 2026-02-23

## Objective

Move from Phase 1 text-hooked schema checks toward direct compatibility-critical
read-paths:

1. start `AipsIO`-compatible read primitives
2. move table metadata extraction toward direct file-format reads (including table endianness metadata)
3. broaden typed payload and `Record` coverage (including array-valued types)

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P2-W1` | `AipsIO` read primitive scaffold | Completed | `aipsio_reader` module with canonical scalar/string/complex decode + object-header decode + tests |
| `P2-W2` | Direct table metadata read path | Completed | `table_dat` header parser (`Table` object, row-count, endian flag, table type) with fixture-backed tests |
| `P2-W3` | Typed hash expansion | Completed | complex + multidimensional array typed payload hashing in oracle tooling with automated checks |
| `P2-W4` | `Record` type matrix expansion | Completed | array-valued and additional compatibility-required value types with multidimensional shape semantics |
| `P2-W5` | Corpus and CI expansion | Completed | additional non-replay artifacts in automated schema/payload checks |
| `P2-W6` | Phase 2 exit review | Pending | risk update and Phase 3 scope recommendation |

## `P2-W1` scope details

Implemented now:

- canonical big-endian primitive reads (`u16/u32/u64`, signed counterparts, `f32/f64`)
- complex reads (`complex64` as two `f32`, `complex128` as two `f64`)
- TypeIO-style string reads (`uInt` length + raw bytes)
- `AipsIO` object header reads (`magic`, object length, object type, object version)
- compile-time host guard: little-endian host only (on-disk canonical big-endian decode retained)

Deliberately deferred from `P2-W1`:

- bit-packed bool-array compatibility behavior
- nested object length-accounting checks (`getstart/getend` equivalence)
- write-path semantics

## Endianness note (carry-forward)

- casacore table storage managers can persist data in either big-endian or
  little-endian canonical form.
- `casacore-mini` host support is currently little-endian only (compile-time
  guard), but table read compatibility must support both on-disk endianness
  variants.

## `P2-W2` scope details

Implemented now:

- direct parser for `table.dat` top-level metadata:
  - `Table` object version
  - row count
  - persisted table-endian flag (`big` or `little`)
  - table implementation type string (`PlainTable` in current fixtures)
- fixture-backed tests against vendored casacore test artifacts (`tTable_2` v0/v1)
- no dependence on `showtableinfo` text formatting for these checks

## `P2-W3` scope details

Implemented now:

- typed payload hashing now supports:
  - scalar `Complex` and `DComplex`
  - complex-valued array payloads
  - multiline matrix-style TaQL array output (`Axis Lengths: [...]` + bracket body)
  - slice-style multidimensional TaQL output (`Ndim=... Axis Lengths` with index/value slice lines)
- parser upgraded from line-based to value-block extraction, so multiline array values are normalized before hashing
- dedicated automated check added:
  - `tools/check_phase2_typed_hash.py`
  - `tools/check_phase2.sh`
  - wired into local and CI quality workflows

## `P2-W4` scope details

Implemented now:

- `RecordValue` scalar type matrix expanded to include:
  - `int16`, `uint16`, `int32`, `uint32`, `int64`, `uint64`
  - `float`, `double`
  - `complex<float>`, `complex<double>`
- `RecordValue` now supports typed multidimensional arrays with explicit shape:
  - integer arrays (`int16/32/64`, `uint16/32/64`)
  - floating arrays (`float`, `double`)
  - string arrays
  - complex arrays (`complex<float>`, `complex<double>`)
- array semantics are explicit:
  - flattened Fortran-order (`first axis` varying fastest)
  - constructor and read-path shape validation (shape-product must match element count)
- `record_io` binary encoding expanded with new value tags and deterministic shape+payload encoding for all supported scalar/array alternatives
- tests expanded to cover:
  - scalar type preservation across round-trip
  - multidimensional array round-trip and type preservation
  - constructor-level invalid shape rejection
  - malformed array payload rejection on deserialize

## `P2-W5` scope details

Implemented now:

- corpus manifest now includes vendored non-replay `table.dat` artifacts:
  - `table_dat_ttable2_v0`
  - `table_dat_ttable2_v1`
- manifest and tooling extended with `source.kind=fixture_path` for in-repo
  non-replay fixtures
- oracle tooling supports direct `table.dat` fixture extraction:
  - parses `AipsIO` table header metadata (table version, row count, endian flag,
    table type)
  - emits deterministic schema/data blocks and metadata hashes
- new automated non-replay Phase 2 corpus check:
  - `tools/check_phase2_non_replay.py`
  - verifies deterministic oracle output (`run1` vs `run2`) and expected decoded
    metadata values for both vendored fixtures
- quality wiring updated:
  - `tools/check_phase2.sh` now runs typed-hash checks and non-replay corpus checks
  - `tools/run_quality.sh` and CI workflow pass build-dir context into Phase 2 checks

## Exit criteria for `P2-W1`

1. Reader tests validate deterministic decode of primitive and object-header paths.
2. Invalid magic and truncated input are rejected with exceptions.
3. New module is wired into existing strict quality gates.
