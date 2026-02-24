# Phase 6 Exit Report

Date: 2026-02-23

## Deliverables completed

### W1: API hardening
- Added allocation plausibility guards to `read_aipsio_array`, `read_aipsio_bool_array`,
  `read_record_desc`, `read_iposition`, and `AipsIoReader::read_string`.
- Guards check claimed element/field counts against `reader.remaining()` before allocation.
- Added tests: `test_rejects_huge_array_count`, `test_rejects_huge_field_count`,
  `test_rejects_negative_field_type`.

### W4: Composite AipsIO Record Writer
- Added `AipsIoWriter::patch_u32()` for object-length back-patching.
- Created `write_aipsio_record()` in `aipsio_record_writer.hpp/cpp`.
- Supports all RecordValue scalar types (bool, i16, u16, i32, u32, i64, float, double,
  complex<float>, complex<double>, string), all array types, and nested sub-records.
- Object lengths are correctly patched for Record, RecordDesc, IPosition, and Array objects.
- Tests: scalar round-trip, array round-trip, nested record, complex types, empty record,
  deterministic output, object length validation.

### W2: Corpus-backed binary record fixtures
- Created `aipsio_fixture_gen.cpp` tool to generate binary `.bin` fixtures.
- Generated 4 fixture files matching existing `showtableinfo.txt` structures:
  - `logtable_stdstman_keywords/column_TIME_keywords.bin`
  - `ms_tree/table_keywords.bin`
  - `ms_tree/column_UVW_keywords.bin`
  - `pagedimage_coords/table_keywords.bin`
- Provenance document with SHA256 checksums and explicit source statement:
  fixtures are casacore-mini-generated (not casacore-produced).

### W3: Binary keyword read integration
- Created `extract_record_metadata()` in `record_metadata.hpp/cpp`.
- Extracts `MeasureCoordinateMetadata` from binary-decoded `Record` objects.
- Same output struct as the text-based `parse_showtableinfo_measure_coordinate_metadata`.
- Cross-validated against text path for all three corpus fixtures (logtable, ms_tree,
  pagedimage_coords).
- Integration is currently in dedicated compatibility tests; live `table_schema`
  path integration is deferred.

### W5: Table write bootstrap
- Created `write_table_dat_header()` and `serialize_table_dat_header()`.
- Writes version-2 format `table.dat` headers.
- Round-trip test validates all fields through `parse_table_dat_metadata`.
- Fixture prefix test validates byte-level compatibility with existing corpus fixture
  (excluding object_length field which covers the full file body).
- Scope for this phase is header-only (`table.dat` top-level metadata), not full
  table directory/body emission.

### W6: Phase 6 compatibility checks
- Created `tools/check_phase6.sh` validating fixture presence and test passage.
- Wired into `tools/run_quality.sh` and `.github/workflows/quality.yml`.

## What is compatible now

- Binary Record **write path**: `write_aipsio_record` produces streams that
  `read_aipsio_record` consumes correctly for all supported types.
- Binary Record **metadata extraction**: `extract_record_metadata` produces
  identical `MeasureCoordinateMetadata` as the text-based path.
- **table.dat header write**: produces byte-compatible headers readable by
  `parse_table_dat_metadata`.
- **Malformed input resilience**: plausibility guards prevent huge allocations
  from corrupted element/field counts.

## Known limitations

1. **Bool array write fidelity**: Decoded bool arrays become `int32_array` in
   `RecordValue` and are written back as `Array<Int>`, not `Array<Bool>` with
   bit-packing. A true bool array round-trip requires a dedicated bool array type.
2. **uint64_t scalars**: No casacore `DataType` code exists for unsigned 64-bit.
   `write_aipsio_record` throws on `uint64_t` values.
3. **table.dat body**: Only the header is written (row count, endian, type).
   TableDesc, column descriptions, and storage manager metadata are not emitted.
4. **Fixtures are self-generated**: Binary fixtures are produced by our own writer,
   not by casacore itself. True interop validation requires casacore-produced binaries.

## Remaining gaps

- Full `table.dat` body (TableDesc, column metadata, storage manager records).
- Storage manager data file emission (`.f0`, `.f1`, etc.).
- Integration of `extract_record_metadata` into the live `table_schema` pipeline
  (currently only used in dedicated tests).
- Casacore-produced binary fixtures for true cross-implementation validation.
- `uint64_array` writing (no casacore encoding exists).

## Misses observed

1. Planning claim mismatch: `P6-W2` originally stated casacore-produced fixtures,
   but implementation produced casacore-mini-generated fixtures.
   - Category: planning scope / interoperability evidence
   - Carry-forward action: obtain and pin casacore-produced binary fixtures in Phase 7.
2. Integration claim mismatch: `P6-W3` was marked done without live pipeline
   integration; extraction path currently runs in dedicated tests only.
   - Category: code integration / phase accounting
   - Carry-forward action: wire binary metadata extraction into active schema
     pipeline in Phase 7.
3. Scope wording mismatch: `P6-W5` wording implied directory/files emission,
   while implementation is header-only `table.dat` bootstrap.
   - Category: docs / planning scope
   - Carry-forward action: keep scope labels explicit (`header-only`, `full-body`,
     `directory-level`) in future phase plans.
4. Closeout completeness gap: initial Phase 6 closeout omitted the candidate
   matrix expected by the plan.
   - Category: closeout process
   - Carry-forward action: include candidate matrix below and keep it required in
     future closeouts.

## Phase 7 Prove-Out Candidate Matrix

| Artifact | Producer | Consumer | Verification target |
|---|---|---|---|
| `Record` keyword binaries (`.bin`) | casacore | casacore-mini (`read_aipsio_record`, `extract_record_metadata`) | typed-field fidelity and metadata parity vs casacore/oracle output |
| `Record` keyword binaries (`.bin`) | casacore-mini (`write_aipsio_record`) | casacore | parse success and semantic equivalence for scoped supported types |
| `table.dat` header+body | casacore-mini | casacore and casacore-mini | header parity + body parse correctness (`TableDesc`, column descriptors, managers) |
| table directory + manager files (`table.f*`) | casacore-mini | casacore | table open/read success and schema/data fidelity on scoped fixtures |
| representative MeasurementSet subset | casacore and casacore-mini | both | bidirectional interpretation of table keywords/column keywords and selected rows |

## Recommended Phase 7 scope

1. **Bidirectional fixture validation**: obtain casacore-produced binary keyword
   Record files and verify `read_aipsio_record` + `extract_record_metadata` match.
2. **Full table.dat write**: emit TableDesc, column descriptions.
3. **Live pipeline integration**: wire `extract_record_metadata` into the active
   metadata extraction code path alongside the text parser.
4. **Storage manager write bootstrap**: minimal StandardStMan header emission.
