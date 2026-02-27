# P10-W9 Review: Utility-layer parity and integration cleanup

## Scope
Implemented 11 missing API-surface items from the W9 utility-layer parity wave,
covering lattice utilities, image utilities, coordinate system transforms,
and supporting types.

## New Types Added

### Lattice layer (`lattice.hpp`)
- **`LatticeConcat<T>`** — Read-only concatenation of N lattices along one axis.
  Validates shape compatibility on non-concat dimensions.
- **`RebinLattice<T>`** — Read-only downsampling by integer factors per axis.
  Uses bin-averaging. Handles non-divisible shapes via ceil rounding.

### Image layer (`image.hpp`)
- **`ImageExpr<T>`** — Read-only image wrapping a LatticeArray (expression result)
  with coordinate system, units, and metadata.
- **`ImageConcat<T>`** — Image concatenation using LatticeConcat internally.
  Inherits units/info from first image.
- **`ImageBeamSet`** — Per-channel/per-Stokes multi-beam storage with Record
  serialization. Supports single-beam and nchan×nstokes grid modes.
- **`MaskSpecifier`** — Simple struct specifying which named mask to use.
- **`ImageSummary`** — Print utility for image metadata (name, shape, units, beam, object).
- **`image_utilities` namespace** — `statistics()`, `copy_image()`, `scale_image()`.
- **`image_regrid()`** — Nearest-neighbor regridding using coordinate system transforms.

### Coordinate layer (`coordinate_system.hpp` / `.cpp`)
- **`CoordinateSystem::to_world()`** — Aggregate pixel→world conversion via
  per-coordinate scatter-gather through world_map/pixel_map.
- **`CoordinateSystem::to_pixel()`** — Inverse world→pixel conversion.

### Non-template implementations (`image.cpp`)
- `ImageBeamSet` constructors, getters, setters, and Record roundtrip.

## Test Coverage
- `tests/utility_parity_test.cpp` — 150 checks across 15 test functions.
- All 76 tests pass (full regression).

## Files Changed
- `include/casacore_mini/lattice.hpp` (LatticeConcat, RebinLattice)
- `include/casacore_mini/image.hpp` (ImageExpr, ImageConcat, ImageBeamSet,
  MaskSpecifier, ImageSummary, image_utilities, image_regrid)
- `include/casacore_mini/coordinate_system.hpp` (to_world, to_pixel decls)
- `src/coordinate_system.cpp` (to_world, to_pixel implementations)
- `src/image.cpp` (ImageBeamSet non-template impls)
- `tests/utility_parity_test.cpp` (new)
- `CMakeLists.txt` (new test target)
- `tools/check_phase10_w9.sh` (gate script)
