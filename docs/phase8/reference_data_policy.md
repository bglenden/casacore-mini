# Phase 8 Reference Data Policy

Date: 2026-02-24

## Overview

Astronomical measure conversions require external reference data for Earth
orientation, leap seconds, and ephemerides. This document defines how
casacore-mini handles this data in Phase 8.

## Principle: Vendored Snapshots, No Network Fetches

All reference data is vendored as frozen snapshots in `data/reference/` with
explicit provenance dates. The library never fetches data from the network at
runtime. This ensures:

1. **Reproducible builds and tests** — identical results regardless of network
   state or upstream data updates.
2. **CI reliability** — no test failures due to network timeouts or data updates.
3. **Deterministic interop** — both casacore and casacore-mini can be tested against
   the same frozen snapshot.

## Data Files

### Leap Second Table

- **File:** `data/reference/leapsec.dat`
- **Format:** Text file matching the IERS Bulletin C format
- **Provenance:** IERS Bulletin C, frozen at a specific issue date
- **Content:** UTC-TAI offset table (leap second insertion dates and cumulative offsets)
- **Update policy:** Manual update only; provenance date recorded in file header

### IERS Earth Orientation Parameters

- **File:** `data/reference/iers_finals2000A.dat`
- **Format:** IERS finals2000A format (fixed-width columns)
- **Provenance:** IERS Earth Orientation Centre, frozen at a specific date
- **Content:** Polar motion (x_p, y_p), UT1-UTC, celestial pole offsets
- **Update policy:** Manual update only; provenance date recorded in file header
- **Scope:** Required only for conversions involving UT1 (e.g., UTC->UT1, apparent
  place calculations, sidereal time)

## Discovery and Loading

### Environment Variable: `CASACORE_MINI_DATA_DIR`

If set, the library looks for reference data files in this directory first.
This allows users to provide updated data or override the vendored snapshot.

### Fallback: Compile-Time Path

If the environment variable is not set, the library uses the compile-time path
to the vendored data in the source tree. The CMake build sets
`CASACORE_MINI_DATA_DIR_DEFAULT` to `${CMAKE_SOURCE_DIR}/data/reference`.

### Load Order

1. `$CASACORE_MINI_DATA_DIR/<filename>` if env var is set
2. `CASACORE_MINI_DATA_DIR_DEFAULT/<filename>` (compile-time path)
3. Throw `MissingReferenceDataError` if file not found

## Failure Semantics

### `MissingReferenceDataError`

A typed exception (`std::runtime_error` subclass) thrown when a conversion
requires reference data that is not available. The error message includes:

- The required data file name
- The conversion being attempted
- The search paths tried

### Conversions That Do Not Require Reference Data

The following conversions work without any reference data files:

- **Epoch:** UTC <-> TAI (leap seconds are required, but the table is always
  vendored), TAI <-> TT (fixed offset: TT = TAI + 32.184s)
- **Direction:** J2000 <-> GALACTIC (fixed rotation matrix), J2000 <-> ICRS
  (identity for practical purposes)
- **Position:** WGS84 <-> ITRF (pure geometric transform via ERFA)
- **Doppler:** All doppler formula conversions (purely mathematical)
- **Frequency:** REST frame (identity)

### Conversions That Require Reference Data

- **Epoch:** UTC <-> UT1 (requires IERS delta-UT1)
- **Direction:** J2000 <-> APPARENT (requires precession/nutation which needs epoch
  and optionally IERS corrections for highest precision)
- **Direction:** Any conversion involving HADEC, AZEL (requires observer
  position + epoch + Earth orientation)
- **Frequency:** TOPO <-> LSRK, BARY, GEO (requires observer position + epoch for
  velocity correction)

### Stale Data Behavior

The library does not check for data staleness. If the vendored snapshot does not
cover the epoch being converted (e.g., future dates beyond the IERS prediction
window), the conversion proceeds with the best available data. This matches
upstream casacore behavior.

## Tolerance Budget for Reference-Data-Dependent Conversions

Reference-data-dependent conversions have larger tolerance budgets than
reference-data-independent ones:

| Conversion | Tolerance | Notes |
|-----------|-----------|-------|
| UTC <-> UT1 | 1e-6 days (~86 ms) | IERS prediction accuracy limit |
| J2000 <-> APPARENT | 1e-7 rad (~20 mas) | Precession/nutation model differences |
| TOPO <-> LSRK | 1e-3 Hz | Velocity correction chain |

These tolerances are documented in `docs/phase8/tolerances.md`.

## Test Strategy

1. **Vendored snapshot tests:** All conversion parity tests use the vendored
   snapshot. Both casacore and casacore-mini are configured to use the same
   snapshot for interop matrix tests.
2. **Absence tests:** Explicit tests verify that `MissingReferenceDataError` is
   thrown when required data files are missing, and that non-data-dependent
   conversions still succeed.
3. **Fixture vectors:** Pre-computed conversion results (generated once using
   casacore with the vendored snapshot) are stored in
   `data/corpus/fixtures/phase8_measures/` as JSON files.
