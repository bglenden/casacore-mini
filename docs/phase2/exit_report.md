# Phase 2 Exit Report (`P2-W6`)

Date: 2026-02-23

## Outcome summary

Phase 2 completed the targeted shift from text-hooked scaffolding toward
compatibility-critical direct read paths:

1. canonical big-endian `AipsIO` read primitives (including object headers and
   complex values)
2. direct `table.dat` metadata parsing with explicit table-endian extraction
3. expanded typed payload hashing for complex and multidimensional array values
4. expanded `RecordValue` type matrix, including shaped multidimensional arrays
5. non-replay corpus checks and CI wiring for direct fixture validation

## Deliverables completed

- `AipsIO` reader:
  - `include/casacore_mini/aipsio_reader.hpp`
  - `src/aipsio_reader.cpp`
  - `tests/aipsio_reader_test.cpp`
- direct table metadata reader:
  - `include/casacore_mini/table_dat.hpp`
  - `src/table_dat.cpp`
  - `tests/table_dat_test.cpp`
- expanded record type matrix and binary encoding:
  - `include/casacore_mini/record.hpp`
  - `include/casacore_mini/record_io.hpp`
  - `src/record.cpp`
  - `src/record_io.cpp`
  - `tests/record_io_test.cpp`
- Phase 2 checks:
  - `tools/check_phase2_typed_hash.py`
  - `tools/check_phase2_non_replay.py`
  - `tools/check_phase2.sh`
- CI/local quality wiring:
  - `.github/workflows/quality.yml`
  - `tools/run_quality.sh`

## Documentation completeness check

Phase 2 public APIs now have Doxygen coverage with explicit semantics,
invariants, and failure modes in:

- `include/casacore_mini/aipsio_reader.hpp`
- `include/casacore_mini/record.hpp`
- `include/casacore_mini/record_io.hpp`
- `include/casacore_mini/table_dat.hpp`

Documentation quality gates are now enforced:

- Doxygen warnings-as-errors (`docs/Doxyfile.in`)
- CI docs job (`.github/workflows/quality.yml`)
- local docs step in `tools/run_quality.sh`

## Remaining gaps

1. Phase 2 still uses transitional schema/keyword extraction from
   `showtableinfo` text in oracle tooling.
2. `Record` binary I/O remains project-local scaffolding and is not yet
   `AipsIO` wire-compatible.
3. A typed C++ extraction module for measures/coordinates metadata is not yet
   implemented (Phase 3 starts with replayed keyword fixture checks).
4. Direct read coverage remains limited to table top-level metadata and does not
   yet include broader storage-manager payload decoding.

## Top risks entering Phase 3

1. Measure keyword schema diversity:
   - casacore encodes measure metadata across multiple keyword conventions
     (`MEASINFO`, `MEASURE_TYPE`, units arrays, and nested records).
2. Coordinate-record complexity:
   - image coordinate records are nested and heterogeneous; brittle ad-hoc
     parsing could create false parity confidence.
3. Corpus drift:
   - if Phase 3 checks rely only on path-local artifacts, CI reproducibility
     weakens; replayed fixtures and deterministic checks are needed.

## Recommended Phase 3 scope

1. Add replay fixtures and deterministic checks for measure- and
   coordinate-rich keyword structures (`logtable`, `MS`, `PagedImage`).
2. Introduce first-class Phase 3 validation scripts and CI wiring dedicated to
   measures/coordinates metadata parity.
3. Define a narrow initial compatibility contract for measures/coordinates
   metadata extraction (presence and key-value semantics before full numeric
   transform behavior).
4. Start a typed in-memory metadata representation for measures/coordinates that
   can later connect to direct `AipsIO`/table keyword reads.
