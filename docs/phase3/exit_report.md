# Phase 3 Exit Report (`P3-W4`)

Date: 2026-02-23

## Outcome summary

Phase 3 established a first compatibility and implementation baseline for
measures/coordinates metadata:

1. deterministic replay fixtures for measure- and coordinate-rich keyword
   structures
2. automated Phase 3 CI/local checks for keyword fixture drift and required
   metadata markers
3. typed extraction contract (`measure_coord_contract_v0`)
4. first C++ read-only extraction API over `showtableinfo` keyword sections with
   fixture-backed tests

## Deliverables completed

- fixtures and provenance:
  - `data/corpus/fixtures/logtable_stdstman_keywords/showtableinfo.txt`
  - `data/corpus/fixtures/ms_tree/showtableinfo.txt`
  - `data/corpus/fixtures/pagedimage_coords/showtableinfo.txt`
  - `data/corpus/fixtures/phase3_keyword_fixtures_provenance.md`
- Phase 3 checks:
  - `tools/check_phase3_keywords.py`
  - `tools/check_phase3.sh`
  - wiring in `tools/run_quality.sh` and `.github/workflows/quality.yml`
- typed contract:
  - `docs/phase3/measure_coord_contract_v0.md`
- C++ API/module:
  - `include/casacore_mini/measure_coord_metadata.hpp`
  - `src/measure_coord_metadata.cpp`
  - `tests/measure_coord_metadata_test.cpp`

## What is compatible now

Current API scope (`parse_showtableinfo_measure_coordinate_metadata`) covers:

1. column-level measure metadata:
   - `UNIT`
   - `QuantumUnits`
   - `MEASURE_TYPE` / `MEASURE_REFERENCE`
   - `MEASINFO.type` / `MEASINFO.Ref`
2. coordinate keyword metadata:
   - `coords` presence
   - `coords.obsdate.type`
   - `coords.obsdate.refer`
   - `coords.direction0.system`
   - `coords.direction0.axes`

## Remaining gaps

1. parser source is still `showtableinfo` text, not direct binary keyword read
   paths.
2. typed extraction does not yet materialize general nested keyword records.
3. no direct integration yet with measures/coordinates numeric transform APIs.
4. `MeasurementSet`-wide subtable measure metadata extraction/parity is not yet
   automated beyond selected main-table markers.

## Top risks entering Phase 4

1. text-source dependency:
   - prolonged reliance on `showtableinfo` can hide low-level binary parity
     issues.
2. partial keyword coverage:
   - narrow extraction may miss metadata forms used by broader datasets.
3. integration lag:
   - delaying image/lattice metadata integration may create late-stage API
     reshaping costs.

## Recommended Phase 4 scope

1. Introduce a typed keyword-record parser for nested table/image keywords as a
   reusable internal representation.
2. Expand image-centric metadata extraction to include additional coordinate
   fields needed for common image workflows.
3. Begin transitioning selected metadata paths from `showtableinfo` text to
   direct table keyword read mechanisms where practical.
4. Add fixture-backed checks for expanded image/lattice keyword parity and
   integrate them into CI/local quality gates.
