# Phase 8 Numeric Tolerance Specifications

Date: 2026-02-24

## Purpose

This document defines the numeric tolerances used for parity testing between
casacore and casacore-mini measure conversions and coordinate transforms.
All interop matrix cells and conversion parity tests must satisfy these
tolerances.

## Measure Tolerances

### Epoch (MEpoch)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| UTC <-> TAI | 1e-12 | days | Exact integer-second offset; float precision limit |
| TAI <-> TT | 1e-12 | days | Fixed offset (32.184s); float precision limit |
| UTC <-> UT1 | 1e-6 | days | IERS prediction accuracy (~86 ms) |
| TT <-> TDB | 1e-10 | days | ERFA model precision |

### Direction (MDirection)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| J2000 <-> GALACTIC | 1e-9 | rad | Fixed rotation matrix; float precision |
| J2000 <-> ECLIPTIC | 1e-9 | rad | Obliquity model precision |
| J2000 <-> ICRS | 1e-12 | rad | Near-identity; frame tie precision |
| J2000 <-> APPARENT | 1e-7 | rad | Precession/nutation model (~20 mas) |
| HADEC/AZEL | 1e-6 | rad | Full chain: Earth orientation + sidereal time |

### Position (MPosition)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| WGS84 <-> ITRF | 1e-3 | m | ERFA geodetic/geocentric precision |

### Frequency (MFrequency)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| REST (identity) | 1e-10 | Hz | Exact |
| LSRK <-> BARY | 1e-3 | Hz | Velocity correction chain |
| TOPO <-> GEO | 1e-3 | Hz | Earth rotation velocity |

### Doppler (MDoppler)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| All conversions | 1e-12 | dimensionless | Purely mathematical formulas |

### Radial Velocity (MRadialVelocity)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| Frame conversions | 1e-3 | m/s | Velocity correction chain |

### Baseline (MBaseline) / UVW (Muvw)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| Frame rotations | 1e-6 | m | Rotation matrix precision |

### Earth Magnetic (MEarthMagnetic)

| Operation | Tolerance | Unit | Rationale |
|----------|-----------|------|-----------|
| Serialization round-trip | 1e-12 | T | Exact round-trip |
| IGRF conversion | 1e-3 | T | Model coefficient precision (if implemented) |

## Coordinate Tolerances

### Pixel/World Transforms

| Coordinate Type | Tolerance | Unit | Rationale |
|----------------|-----------|------|-----------|
| DirectionCoordinate (pixel <-> world) | 1e-10 | rad/pixel | WCSLIB precision |
| SpectralCoordinate (pixel <-> world) | 1e-10 | Hz/pixel | Linear arithmetic precision |
| StokesCoordinate | exact | integer | Discrete mapping |
| LinearCoordinate | 1e-10 | world units/pixel | Matrix arithmetic precision |
| TabularCoordinate | 1e-10 | world units/pixel | Interpolation precision |
| QualityCoordinate | exact | integer | Discrete mapping |

### CoordinateSystem Record Parity

| Field | Tolerance | Notes |
|-------|-----------|-------|
| String fields (type, system, projection) | exact | Case-sensitive match |
| crval, crpix, cdelt, pc | 1e-12 | Relative tolerance |
| longpole, latpole | 1e-10 | Degrees; WCSLIB internal precision |
| Axis mapping arrays | exact | Integer arrays |
| Integer fields | exact | |

## Application in Tests

Tests use these tolerance constants defined in test headers:

```cpp
constexpr double kEpochTolDays = 1e-12;
constexpr double kDirectionTolRad = 1e-9;
constexpr double kPositionTolM = 1e-3;
constexpr double kFrequencyTolHz = 1e-6;
constexpr double kCoordPixelWorldTol = 1e-10;
```

Comparison is absolute for small values and relative for large values:
`|a - b| <= tol * max(1.0, max(|a|, |b|))`.
