# Phase 8 Interop Artifact Inventory

Date: 2026-02-24

## Overview

Phase 8 requires 6 distinct artifact types in the 2x2 producer/consumer interop
matrix (casacore x casacore-mini). Each artifact must pass all 4 matrix cells:

1. casacore -> casacore (self-check)
2. casacore -> casacore-mini (cross-read)
3. casacore-mini -> casacore-mini (self-check)
4. casacore-mini -> casacore (cross-read)

Total matrix cells: 6 artifacts x 4 directions = 24 cells.

## Artifact 1: Scalar Measure-Column Table

**Description:** Table with scalar measure columns for TIME (MEpoch) and DIRECTION
(MDirection), with MEASINFO keywords and QuantumUnits.

**Contents:**
- 10 rows of data
- `TIME` column: MEpoch values (MJD, UTC reference), with MEASINFO `{type: "epoch", Ref: "UTC"}`
- `DIRECTION` column: MDirection values (2-element Double, J2000), with MEASINFO `{type: "direction", Ref: "J2000"}`
- QuantumUnits on TIME: `["s"]`
- QuantumUnits on DIRECTION: `["rad", "rad"]`

**Interop commands:** `produce-measure-table` / `verify-measure-table`

**Semantic checks:**
- MEASINFO keyword structure parity
- QuantumUnits parity
- Measure type and reference string parity
- Numeric value parity within tolerance (epoch: 1e-12 days, direction: 1e-9 rad)

## Artifact 2: Array Measure-Column Table

**Description:** Table with array measure columns for UVW (Muvw).

**Contents:**
- 10 rows of data
- `UVW` column: Muvw array values (3-element Double per row), ITRF reference
- MEASINFO: `{type: "uvw", Ref: "ITRF"}`
- QuantumUnits: `["m", "m", "m"]`

**Interop commands:** `produce-array-measure-table` / `verify-array-measure-table`

**Semantic checks:**
- Array shape parity (3 elements per row)
- MEASINFO keyword parity
- Numeric value parity (position tolerance: 1e-3 m)

## Artifact 3: Rich Measure-Keyword Table

**Description:** Table with complex measure metadata in table and column keywords,
including variable-reference columns, offset measures, and multiple measure types.

**Contents:**
- Table keywords with embedded measure records (MeasureHolder format)
- Column keywords with MEASINFO for multiple measure types
- Variable-reference column (VarRefCol, TabRefTypes, TabRefCodes)
- Offset measure in one column's MEASINFO

**Interop commands:** `produce-measure-keywords` / `verify-measure-keywords`

**Semantic checks:**
- Table keyword Record structure parity
- Column keyword MEASINFO structure parity
- Variable-reference column metadata parity
- Offset measure Record parity
- All string values (type names, reference names) case-insensitive match

## Artifact 4: Coordinate-Rich Image Metadata

**Description:** Table with full CoordinateSystem in table keywords, matching the
layout used by casacore PagedImage.

**Contents:**
- Table keywords containing a `coords` sub-record with:
  - `direction0`: DirectionCoordinate (J2000, SIN projection)
  - `spectral1`: SpectralCoordinate (LSRK, linear channels)
  - `stokes2`: StokesCoordinate (I, Q, U, V)
  - ObsInfo (telescope, observer, obsdate)
  - Axis mapping arrays (worldmap, pixelmap, worldreplace, pixelreplace)

**Interop commands:** `produce-coord-keywords` / `verify-coord-keywords`

**Semantic checks:**
- CoordinateSystem Record structure parity
- Per-coordinate Record field parity (crval, crpix, cdelt, pc, projection, etc.)
- ObsInfo field parity
- Axis mapping array parity
- World/pixel transform parity for 5 deterministic sample points per coordinate

## Artifact 5: Mixed Coordinate System

**Description:** Table with a complex coordinate system containing all 6 coordinate
types: direction + spectral + stokes + linear + tabular + quality.

**Contents:**
- Table keywords with `coords` sub-record containing:
  - `direction0`: J2000/TAN projection
  - `spectral1`: TOPO reference, 256 channels
  - `stokes2`: I, Q
  - `linear3`: 2-axis generic linear coordinate
  - `tabular4`: 10-point lookup table
  - `quality5`: DATA/ERROR quality axis
  - Full axis mapping arrays

**Interop commands:** `produce-mixed-coords` / `verify-mixed-coords`

**Semantic checks:**
- All 6 coordinate types present and correctly typed
- Record structure parity for each coordinate type
- World/pixel round-trip parity for each coordinate
- System-level axis mapping parity

## Artifact 6: Reference-Data Conversion Vectors

**Description:** JSON file containing pre-computed conversion results for
deterministic test vectors, using the vendored reference-data snapshot.

**Contents:**
- Epoch conversions: MJD 59000.5 UTC -> TAI, TT, UT1 (with IERS data)
- Direction conversions: RA=180d, Dec=45d J2000 -> GALACTIC, ECLIPTIC
- Position conversions: lon=-118d, lat=34d, h=100m WGS84 -> ITRF
- Frequency conversions: 1.4 GHz REST -> LSRK (with frame context)
- Doppler conversions: v=1000 km/s RADIO -> Z, BETA, OPTICAL

**Interop commands:** `produce-conversion-vectors` / `verify-conversion-vectors`

**Semantic checks:**
- All conversion results within documented tolerances
- Reference-data provenance match (same IERS snapshot date)
- Failure modes match when reference data is absent

## Fixture Storage

Interop artifacts are produced at test time into `${BUILD_DIR}/phase8_artifacts/`.
Pre-computed reference vectors are stored in `data/corpus/fixtures/phase8_measures/`.

## Matrix Runner

`tools/interop/run_phase8_matrix.sh` executes all 24 matrix cells and reports
pass/fail with machine-readable JSON output.
