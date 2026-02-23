# Phase 5 Exit Report

Date: 2026-02-23

## Outcome summary

Phase 5 established write-path foundations for AipsIO metadata while
maintaining full read-path compatibility:

1. defined a keyword-record fidelity contract (v1) specifying the scalar type
   and array shape requirements for write-safe metadata
2. implemented `AipsIoWriter` as the big-endian canonical write counterpart to
   `AipsIoReader`, with round-trip tests for all primitive types and object
   headers
3. implemented `read_aipsio_record` to decode casacore `Record` objects
   directly from AipsIO binary streams, covering all scalar types, arrays
   (including bit-packed bools), complex values, string arrays, and nested
   sub-records
4. added Phase 5 compatibility checks and wired them into local/CI quality
   pipelines

## Deliverables completed

- fidelity contract:
  - `docs/phase5/keyword_record_fidelity_v1.md`
- AipsIO write primitives:
  - `include/casacore_mini/aipsio_writer.hpp`
  - `src/aipsio_writer.cpp`
  - `tests/aipsio_writer_test.cpp`
- AipsIO Record reader:
  - `include/casacore_mini/aipsio_record_reader.hpp`
  - `src/aipsio_record_reader.cpp`
  - `tests/aipsio_record_reader_test.cpp`
- Phase 5 checks:
  - `tools/check_phase5.sh`
  - wiring in `tools/run_quality.sh` and `.github/workflows/quality.yml`
- tracking docs:
  - `docs/phase5/plan.md` (kickoff, from Phase 4)
  - `docs/phase5/exit_report.md`

## What is compatible now

1. `AipsIoWriter` produces canonical big-endian byte streams that round-trip
   exactly through `AipsIoReader` for all supported primitive types:
   - u8, i16/u16, i32/u32, i64/u64, f32, f64
   - complex<float>, complex<double>
   - TypeIO-style strings
   - object headers (magic + length + type + version)
2. `read_aipsio_record` decodes casacore `Record` binary streams into
   `Record` objects:
   - all scalar types (bool, short/ushort, int/uint, int64, float, double,
     complex, dcomplex, string); `Char`/`uChar` are losslessly promoted to
     `int16_t`/`uint16_t` (no 8-bit type in `RecordValue`)
   - all array types including bit-packed bool arrays and string arrays
   - nested sub-records (variable and fixed)
   - IPosition v1/v2, Array v2/v3, RecordDesc v1/v2
   - negative size fields in malformed input are detected and rejected
3. `RecordValue` type system (from Phase 2) satisfies full scalar width and
   array shape fidelity required by the fidelity contract.
4. Write encoding is deterministic (identical inputs produce identical bytes).
5. Boundary values (min/max for all integer widths) round-trip correctly.

## Deferred work

1. `AipsIoWriter` does not yet encode `Record`/`TableRecord` composite
   objects via AipsIO framing — only primitives and object headers are
   supported.
2. no storage-manager payload write support.

## Remaining gaps

1. composite AipsIO object writing (Record, TableRecord, TableDesc) requires
   additional framing logic beyond primitive encode.
2. `read_aipsio_record` has been tested with synthetic wire-format data;
   corpus-backed validation against actual casacore-produced keyword files
   would provide stronger confidence.
3. keyword metadata round-trip has not yet been tested against actual casacore
   corpus fixtures (current round-trip tests are synthetic).
4. no write-path integration with table directory layout or lock metadata.

## Top risks entering Phase 6

1. composite framing complexity:
   - AipsIO object-length fields require either two-pass encoding or
     seek-back patching, neither of which is implemented yet.
2. corpus-backed validation gap:
   - write-path correctness has only been proven against synthetic
     round-trips; fixture-backed comparison against casacore-produced bytes
     would provide stronger confidence.
3. storage-manager write scope:
   - even with metadata write parity proven, storage-manager payload writes
     are a substantial separate effort.

## Recommended Phase 6 scope

1. implement composite AipsIO object writing for `Record` and `TableRecord`
   with corpus-backed round-trip validation.
2. add write-path table directory layout (table.dat, table.info) generation.
3. begin `StandardStMan` write support for scalar columns.
4. integrate `read_aipsio_record` into the keyword read pipeline to replace
   or supplement `showtableinfo` text parsing where binary files are available.
5. validate `read_aipsio_record` against corpus keyword files produced by
   casacore.
