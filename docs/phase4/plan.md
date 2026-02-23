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
| `P4-W1` | Typed keyword-record model v0 | Completed | internal representation for nested keyword records with arrays/scalars and deterministic traversal |
| `P4-W2` | Image coordinate metadata expansion | Completed | extraction of additional `coords.direction0` and related image keyword fields with tests |
| `P4-W3` | Phase 4 compatibility checks | Completed | expanded fixture checks wired into CI/local quality for image/lattice metadata |
| `P4-W4` | Phase 4 exit recommendation | Pending | risk update and Phase 5 write-path preparation scope |

## `P4-W1` scope details

Implemented now:

- define a compact internal keyword-value representation suitable for nested
  records and scalar/array payloads
- keep representation separate from legacy casacore naming and implementation
  internals
- start with read-only extraction from replayed `showtableinfo` fixtures

Current implementation artifacts:

- `include/casacore_mini/keyword_record.hpp`
- `src/keyword_record.cpp`
- `tests/keyword_record_test.cpp`
- `CMakeLists.txt` test/build wiring (`keyword_record_test`)

## Exit criteria for `P4-W1`

1. Typed keyword-record model is implemented with deterministic equality
   semantics.
2. At least one nested image keyword record path is parsed into the model.
3. Tests verify record structure and selected value extraction.

## `P4-W2` scope details

Implemented now:

- migrated `parse_showtableinfo_measure_coordinate_metadata(...)` to traverse
  `parse_showtableinfo_keywords(...)` output instead of using ad-hoc regex
  parsing
- expanded `CoordinateKeywordMetadata` extraction coverage for:
  - `coords.direction0.projection`
  - `coords.direction0.projection_parameters`
  - `coords.direction0.crval`
  - `coords.direction0.crpix`
  - `coords.direction0.cdelt`
  - `coords.direction0.pc`
  - `coords.direction0.units`
  - `coords.direction0.conversionSystem`
  - `coords.direction0.longpole`
  - `coords.direction0.latpole`
  - `coords.worldmap0`
  - `coords.worldreplace0`
  - `coords.pixelmap0`
  - `coords.pixelreplace0`
- extended fixture-backed tests for `pagedimage_coords` to validate the added
  fields

Current implementation artifacts:

- `include/casacore_mini/measure_coord_metadata.hpp`
- `src/measure_coord_metadata.cpp`
- `tests/measure_coord_metadata_test.cpp`

## `P4-W3` scope details

Implemented now:

- added Phase 4 image-keyword fixture guard:
  - `tools/check_phase4_image_keywords.py`
  - validates deterministic keyword-section hash for
    `pagedimage_coords/showtableinfo.txt`
  - asserts presence of expanded `coords.direction0` and mapping markers
- added Phase 4 check entrypoint:
  - `tools/check_phase4.sh`
- wired Phase 4 checks into:
  - `tools/run_quality.sh`
  - `.github/workflows/quality.yml` (`quality / build-lint-test-coverage`)
