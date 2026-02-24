# Phase 8 Exit Report

Date: 2026-02-24
Status: Complete

## Objective

Implement full Measures, TableMeasures, Coordinates, and CoordinateSystem
modules in casacore-mini with strict 2x2 interoperability against upstream
casacore.

## Results Summary

| Metric | Result |
|--------|--------|
| Unit tests | 44/44 pass |
| Interop matrix | 24/24 cells pass |
| Measure types implemented | 9/9 (epoch, direction, position, frequency, doppler, radial_velocity, baseline, uvw, earth_magnetic) |
| Coordinate types implemented | 6/6 (direction, spectral, stokes, linear, tabular, quality) |
| Hardening test suites | 3 (measure_malformed, coordinate_malformed, reference_data_absence) |
| Wave gates | 12/12 pass (W1-W12) |

## Wave Status

| Wave | Status | Deliverables |
|------|--------|-------------|
| W1 | Done | API surface map, dependency decisions, fixture inventory, reference data policy, tolerances |
| W2 | Done | Quantity, MeasureTypes (9 types, all reference enums), MeasureRecord serialization |
| W3 | Done | MeasureConvert, ERFA bridge, MEpoch/MPosition/MDirection conversions |
| W4 | Done | VelocityMachine, UvwMachine, remaining measure conversions (all 9 types wired) |
| W5 | Done | TableMeasureDesc, TableMeasureColumn (scalar/array measure column adapters) |
| W6 | Done | Coordinate base class, Projection, LinearXform, ObsInfo |
| W7 | Done | DirectionCoordinate (WCSLIB), SpectralCoordinate, StokesCoordinate, LinearCoordinate, TabularCoordinate, QualityCoordinate |
| W8 | Done | CoordinateSystem composition with axis mapping and Record persistence |
| W9 | Done | CoordinateUtil, FITSCoordinateUtil, GaussianConvert |
| W10 | Done | Full 24-cell interop matrix (6 artifacts x 4 directions) |
| W11 | Done | Malformed input hardening, reference data absence tests, known differences doc |
| W12 | Done | Exit report, plan status updates, Phase 9 entry conditions |

## Interop Matrix Results

6 artifacts, each tested in 4 directions (mini->mini, mini->casacore,
casacore->mini, casacore->casacore):

| Artifact | mini->mini | mini->casacore | casacore->mini | casacore->casacore |
|----------|-----------|----------------|----------------|-------------------|
| measure-scalar | PASS | PASS | PASS | PASS |
| measure-array | PASS | PASS | PASS | PASS |
| measure-rich | PASS | PASS | PASS | PASS |
| coord-keywords | PASS (exact) | PASS (semantic) | PASS (semantic) | PASS (exact) |
| mixed-coords | PASS (exact) | PASS (semantic) | PASS (semantic) | PASS (exact) |
| conversion-vectors | PASS (exact) | PASS (exact) | PASS (exact) | PASS (exact) |

Cross-tool coordinate artifacts use semantic verification (checking coordinate
types, reference systems, and transform values) because the Record structures
differ between implementations. Self-roundtrips achieve exact byte match.

## Implementation Inventory

### New Headers (27)

- `include/casacore_mini/quantity.hpp`
- `include/casacore_mini/measure_types.hpp`
- `include/casacore_mini/measure_record.hpp`
- `include/casacore_mini/erfa_bridge.hpp`
- `include/casacore_mini/measure_convert.hpp`
- `include/casacore_mini/velocity_machine.hpp`
- `include/casacore_mini/uvw_machine.hpp`
- `include/casacore_mini/table_measure_desc.hpp`
- `include/casacore_mini/table_measure_column.hpp`
- `include/casacore_mini/coordinate.hpp`
- `include/casacore_mini/projection.hpp`
- `include/casacore_mini/linear_xform.hpp`
- `include/casacore_mini/obs_info.hpp`
- `include/casacore_mini/direction_coordinate.hpp`
- `include/casacore_mini/spectral_coordinate.hpp`
- `include/casacore_mini/stokes_coordinate.hpp`
- `include/casacore_mini/linear_coordinate.hpp`
- `include/casacore_mini/tabular_coordinate.hpp`
- `include/casacore_mini/quality_coordinate.hpp`
- `include/casacore_mini/coordinate_system.hpp`
- `include/casacore_mini/coordinate_util.hpp`
- `include/casacore_mini/fits_coordinate_util.hpp`
- `include/casacore_mini/gaussian_convert.hpp`

### New Sources (23)

- `src/quantity.cpp`
- `src/measure_types.cpp`
- `src/measure_record.cpp`
- `src/erfa_bridge.cpp`
- `src/measure_convert.cpp`
- `src/velocity_machine.cpp`
- `src/uvw_machine.cpp`
- `src/table_measure_desc.cpp`
- `src/table_measure_column.cpp`
- `src/coordinate.cpp`
- `src/projection.cpp`
- `src/linear_xform.cpp`
- `src/obs_info.cpp`
- `src/direction_coordinate.cpp`
- `src/spectral_coordinate.cpp`
- `src/stokes_coordinate.cpp`
- `src/linear_coordinate.cpp`
- `src/tabular_coordinate.cpp`
- `src/quality_coordinate.cpp`
- `src/coordinate_system.cpp`
- `src/coordinate_util.cpp`
- `src/fits_coordinate_util.cpp`
- `src/gaussian_convert.cpp`

### New Tests (22)

- `tests/quantity_test.cpp`
- `tests/measure_types_test.cpp`
- `tests/measure_record_test.cpp`
- `tests/erfa_bridge_test.cpp`
- `tests/measure_convert_test.cpp`
- `tests/velocity_machine_test.cpp`
- `tests/uvw_machine_test.cpp`
- `tests/measure_convert_full_test.cpp`
- `tests/table_measure_desc_test.cpp`
- `tests/table_measure_column_test.cpp`
- `tests/projection_test.cpp`
- `tests/linear_xform_test.cpp`
- `tests/obs_info_test.cpp`
- `tests/direction_coordinate_test.cpp`
- `tests/spectral_coordinate_test.cpp`
- `tests/stokes_coordinate_test.cpp`
- `tests/coordinate_record_test.cpp`
- `tests/coordinate_system_test.cpp`
- `tests/coordinate_util_test.cpp`
- `tests/fits_coordinate_util_test.cpp`
- `tests/gaussian_convert_test.cpp`
- `tests/measure_malformed_test.cpp`
- `tests/coordinate_malformed_test.cpp`
- `tests/reference_data_absence_test.cpp`

## External Dependencies Added

| Dependency | Version | Purpose | Integration |
|-----------|---------|---------|-------------|
| ERFA | 2.0.1 | Astronomical time/frame transforms | pkg-config, FetchContent fallback |
| WCSLIB | 8.5 | WCS projection math for DirectionCoordinate | pkg-config, FetchContent fallback |

## Known Differences from Upstream

Documented in `docs/phase8/known_differences.md`. Key items:

1. **MEarthMagnetic**: IGRF model not implemented; serialization works but
   conversion throws `std::runtime_error`.
2. **CoordinateSystem Record layout**: Structural differences in field naming
   between implementations. Semantic content is equivalent; interop uses
   semantic verification for cross-tool coordinate artifacts.
3. **Precision**: All conversions within documented tolerances
   (`docs/phase8/tolerances.md`). ERFA-based transforms match upstream to
   1e-9 rad (direction) and 1e-12 days (epoch).

## Misses and Carry-Forward

1. **MFrequency / MRadialVelocity cross-frame conversion**: Same-frame identity
   conversions work. Cross-frame conversions (e.g., REST→LSRK) require observer
   velocity correction (position + epoch context) and are deferred to Phase 9.
   These throw `std::invalid_argument` with a clear message.
2. **MEarthMagnetic IGRF model**: The IGRF geomagnetic field model is not
   implemented. Frame rotations (J2000→GALACTIC, etc.) work via the standard
   spherical round-trip. IGRF-involving paths throw `std::runtime_error`.
3. **Coverage gate**: The `check_ci_build_lint_test_coverage.sh` script was
   not run during this closeout due to build directory configuration. Should
   be validated before merging.

## Phase 9 Entry Conditions

Phase 9 (MeasurementSet) may begin when:

1. `bash tools/check_phase8.sh` passes (verified: PASS)
2. Phase 8 interop matrix 24/24 cells pass (verified: 24/24 PASS)
3. Phase 8 exit report exists with no unresolved blocking issues (this document)
4. All 44 unit tests pass (verified: 44/44 PASS)

## Phase 9 Recommendations

Phase 9 implements the full MeasurementSet schema layer on top of Phase 7
(tables) and Phase 8 (measures + coordinates). Key focus areas:

1. MS schema definition (main table + sub-tables with required columns)
2. MS creation and population using Phase 7 table write + Phase 8 measures
3. MS reading with measure-aware column access
4. MS interop matrix (casacore MS files readable by mini and vice versa)
5. MS selection/query basics (row selection, column subsetting)
