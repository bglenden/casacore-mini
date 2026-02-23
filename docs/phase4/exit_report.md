# Phase 4 Exit Report (`P4-W4`)

Date: 2026-02-23

## Outcome summary

Phase 4 improved image/coordinate metadata handling and hardened compatibility
checks around typed keyword structures:

1. added a typed nested keyword-record model for replayed `showtableinfo`
   keyword sections
2. migrated measure/coordinate extraction to that typed model and expanded image
   coordinate coverage
3. added dedicated Phase 4 fixture checks and wired them into local/CI quality
   pipelines

## Deliverables completed

- typed keyword model and parser:
  - `include/casacore_mini/keyword_record.hpp`
  - `src/keyword_record.cpp`
  - `tests/keyword_record_test.cpp`
- expanded measure/coordinate metadata extraction:
  - `include/casacore_mini/measure_coord_metadata.hpp`
  - `src/measure_coord_metadata.cpp`
  - `tests/measure_coord_metadata_test.cpp`
- Phase 4 checks:
  - `tools/check_phase4_image_keywords.py`
  - `tools/check_phase4.sh`
  - wiring in `tools/run_quality.sh` and `.github/workflows/quality.yml`
- tracking docs:
  - `docs/phase4/plan.md`
  - `docs/phase5/plan.md` (kickoff)

## What is compatible now

1. nested table/column keyword records can be parsed into deterministic in-memory
   structures from `showtableinfo` fixtures.
2. read-only coordinate extraction now includes:
   - `coords.obsdate.(type, refer)`
   - `coords.direction0.(system, projection, projection_parameters, crval, crpix, cdelt, pc, axes, units, conversionSystem, longpole, latpole)`
   - `coords.(worldmap0, worldreplace0, pixelmap0, pixelreplace0)`
3. local/CI checks fail on fixture drift or missing key image-coordinate markers.

## Remaining gaps

1. source dependency is still `showtableinfo` text, not direct table keyword
   binary read paths.
2. current `KeywordValue` typing is intentionally compact (`bool`, `int64`,
   `double`, `string`, nested/array) and does not preserve full on-disk scalar
   width/type fidelity.
3. `KeywordArray` currently stores values only; explicit persisted shape/layout
   metadata is not modeled.
4. no write-path parity yet for keyword/table metadata encodings.

## Top risks entering Phase 5

1. write-path fidelity risk:
   - lossy intermediate typing can block byte-level or semantic parity for
     encoded keywords.
2. shape/ordering ambiguity:
   - without explicit array shape semantics, multidimensional compatibility can
     regress silently.
3. delayed binary keyword integration:
   - prolonged text reliance can defer discovery of `AipsIO`/table keyword
     corner cases until late.

## Recommended Phase 5 scope

1. introduce a keyword-record v1 contract with explicit scalar-type and array
   shape semantics needed for write/read parity.
2. add `AipsIO` write primitives and round-trip tests against existing read
   paths.
3. start direct table-keyword binary extraction/normalization paths to reduce
   dependence on `showtableinfo`.
4. prepare first write-path fixtures and comparator hooks focused on
   compatibility-critical metadata surfaces before broader table-data writes.
