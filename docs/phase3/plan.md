# Phase 3 Plan (Kickoff)

Date: 2026-02-23

## Objective

Establish compatibility checks and first implementation slices for
measure/coordinate metadata used by common table/MS/image workflows.

Immediate goals:

1. lock deterministic fixtures for measure- and coordinate-rich keyword shapes
2. enforce automated Phase 3 checks in local and CI quality paths
3. define minimal typed extraction contract for metadata needed by downstream
   measures/coordinates implementation

## Documentation gate (carry-forward)

- Any Phase 3 task that introduces or changes public C++ API must include
  Doxygen updates in the same task.
- Phase closure requires passing CI docs generation with Doxygen warnings treated
  as errors.

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P3-W1` | Measure/coordinate keyword fixtures + baseline checks | Completed | replayed `showtableinfo` fixtures (`logtable`, `ms_tree`, `pagedimage_coords`) + deterministic hash/presence checks |
| `P3-W2` | Typed metadata extraction contract v0 | Completed | explicit normalized schema for measure and coordinate keyword paths/values |
| `P3-W3` | Initial C++ metadata extraction module | Completed | read-only extraction API for prioritized metadata fields + tests |
| `P3-W4` | Phase 3 compatibility expansion recommendation | Completed | risk update + recommended Phase 4 integration scope |

## `P3-W1` scope details

Implemented now:

- capture and vendor deterministic `showtableinfo` fixtures for:
  - `logtable_stdstman_keywords`
  - `ms_tree`
  - `pagedimage_coords`
- sanitize fixture paths to `${CASACORE_BUILD_ROOT}` placeholders
- add Phase 3 check script that validates:
  - canonical keyword-section hashes
  - required measure/coordinate marker presence
- wire Phase 3 checks into:
  - `tools/run_quality.sh`
  - `.github/workflows/quality.yml`

## `P3-W2` scope details

Implemented now:

- contract document added at `docs/phase3/measure_coord_contract_v0.md`
- normalized output model defined for:
  - `MeasureColumnMetadata`
  - `CoordinateKeywordMetadata`
- extraction mapping rules and partial-data behavior documented explicitly

## `P3-W3` scope details

Implemented now:

- new API module:
  - `include/casacore_mini/measure_coord_metadata.hpp`
  - `src/measure_coord_metadata.cpp`
- parser entry point:
  - `parse_showtableinfo_measure_coordinate_metadata(...)`
- fixture-backed tests:
  - `tests/measure_coord_metadata_test.cpp`
  - `logtable_stdstman_keywords`, `ms_tree`, `pagedimage_coords` replay fixtures
- build/test wiring:
  - `CMakeLists.txt` adds `measure_coord_metadata_test`

## `P3-W4` scope details

Implemented now:

- Phase 3 exit report added at `docs/phase3/exit_report.md`
- risk update captured for text-source dependency and metadata coverage
- Phase 4 kickoff plan added at `docs/phase4/plan.md`

## Exit criteria for Phase 3

1. Phase 3 fixture artifacts exist in repo with provenance notes.
2. Phase 3 check script is deterministic and fails on fixture drift.
3. Typed measure/coordinate extraction contract and initial C++ API are implemented and tested.
4. Local `tools/run_quality.sh` and CI `quality` workflow run Phase 3 checks.
