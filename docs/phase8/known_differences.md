# Phase 8: Known Differences from Upstream casacore

## Measure Types

### MEarthMagnetic
- IGRF geomagnetic field model is **not implemented**. Serialization and
  record round-trip work. Frame rotations between celestial frames
  (J2000, GALACTIC, ECLIPTIC, ICRS, APP) work via the standard spherical
  direction round-trip. Conversions involving IGRF throw
  `std::runtime_error("IGRF not implemented")`.
- Upstream casacore uses the full IGRF coefficient set for field vector
  computation at arbitrary positions and epochs.

### MFrequency / MRadialVelocity
- **Cross-frame conversions are not implemented**. Same-frame identity
  conversions work (e.g., LSRK→LSRK returns value unchanged).
- Cross-frame conversions (e.g., REST→LSRK, BARY→TOPO) require observer
  velocity correction based on position and epoch context. These throw
  `std::invalid_argument` with a descriptive message noting this is a
  Phase 9 carry-forward item.
- Upstream casacore supports full cross-frame frequency/velocity conversions
  given a populated MeasureFrame.

### MBaseline / Muvw
- Frame conversions work for celestial frames (J2000, ICRS, GALACTIC,
  ECLIPTIC, APP) via spherical direction round-trip. Topocentric and
  site-specific frames (HADEC, AZEL, ITRF, TOPO) are not supported and
  throw `std::invalid_argument`.

### MDirection
- Frame-dependent conversions (J2000→AZEL, J2000→HADEC) require a fully
  populated `MeasureFrame` with both epoch and position. Upstream casacore
  has the same requirement but provides more informative error messages.
- The ~30 direction reference types are all representable but not all
  conversion paths are implemented. Unsupported pairs throw
  `std::invalid_argument`.

### MEpoch
- UTC↔UT1 conversion requires vendored IERS data. When the data file is
  absent, `MissingReferenceDataError` is thrown. Upstream casacore has the
  same requirement but may silently fall back to UTC≈UT1 in some builds.

## Coordinates

### CoordinateSystem Record Layout
- casacore-mini and upstream casacore produce structurally different Record
  layouts for `CoordinateSystem::save()`. The semantic content (coordinate
  types, reference values, transforms) is equivalent, but field naming and
  nesting differ. The interop matrix uses semantic verification to confirm
  equivalence.
- Specific differences:
  - casacore-mini includes `coordinate_type` fields in sub-records;
    upstream uses `ncoordinates` + indexed naming.
  - Axis mapping arrays use the same semantics but may have different
    default values for removed-axis replacements.

### DirectionCoordinate
- WCSLIB projection types are limited to those compiled into the linked
  WCSLIB version. Exotic or deprecated projections (e.g., BON, PCO) may
  not be available.
- `longpole`/`latpole` are stored in degrees in Records (matching upstream).

## Numeric Precision

### Epoch Conversions
- UTC→TAI uses ERFA `eraUtctai` which matches upstream to ~1e-12 days.
- UTC→TT (via TAI+32.184s) matches upstream to ~1e-12 days.
- TDB↔TT uses ERFA `eraDtdb` approximation; matches upstream to ~1e-9 days
  for epochs near J2000.

### Direction Conversions
- J2000→GALACTIC uses a fixed rotation matrix matching IAU 1958 definition;
  agrees with upstream to ~1e-9 rad.
- Precession/nutation (J2000→APP) uses ERFA `eraPn06a`; agrees with upstream
  to ~1e-8 rad for recent epochs.

### Position Conversions
- WGS84↔ITRF uses ERFA `eraGd2gc`/`eraGc2gd`; matches upstream to ~1e-3 m.

## Interop Artifacts
- All 24 matrix cells (6 artifacts × 4 directions) pass with the semantic
  verification fallback for coordinate-bearing artifacts.
- Byte-exact Record equality is achieved for measure-only artifacts
  (measure-scalar, measure-array, measure-rich).
- Coordinate artifacts (coord-keywords, mixed-coords) require semantic
  verification due to the structural Record differences noted above.
