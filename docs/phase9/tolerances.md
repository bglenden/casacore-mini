# Phase 9 Numeric/String/Timestamp Tolerance Policy

## Floating-point tolerances

### Double-precision values (TIME, UVW, frequencies, positions)
- Absolute tolerance: 1e-10
- Relative tolerance: 1e-12
- Rationale: casacore stores doubles in IEEE 754 binary64; round-trip through
  AipsIO preserves full precision. Small tolerance accounts for potential
  intermediate arithmetic differences.

### Single-precision values (DATA, WEIGHT, SIGMA as float)
- Absolute tolerance: 1e-5
- Relative tolerance: 1e-6
- Rationale: float32 has ~7 significant digits.

### Complex values (DATA as complex float)
- Apply float tolerance independently to real and imaginary parts.

### Doppler/velocity values
- Absolute tolerance: 1e-6 m/s
- Relative tolerance: 1e-10

### Direction values (RA/Dec in radians)
- Absolute tolerance: 1e-12 rad (~0.2 microarcseconds)

### Position values (XYZ in meters)
- Absolute tolerance: 1e-6 m (1 micrometer)

## Integer tolerances

All integer values (IDs, counts, flags) must match exactly.
- ANTENNA1, ANTENNA2, FIELD_ID, DATA_DESC_ID, SCAN_NUMBER, etc.: exact match
- FLAG values: exact match (boolean)
- Row counts: exact match

## String tolerances

All string values must match exactly (byte-for-byte).
- Column names, table type strings, comments: exact
- Antenna names, field names: exact
- Measure reference type strings: case-insensitive comparison

## Timestamp tolerances

- MJD timestamps (TIME column): absolute tolerance 1e-10 seconds
  (~0.1 nanoseconds)
- Date string parsing: must produce identical MJD values within tolerance

## Array shape tolerances

Array shapes must match exactly (same dimensions, same extents).

## Selection row-set tolerances

Selected row sets must be identical (same rows in same order).
No tolerance for missing or extra rows.

## Comparison implementation

Use the following comparison function for floating-point:
```
bool approx_equal(double a, double b, double abs_tol, double rel_tol) {
    double diff = std::abs(a - b);
    return diff <= abs_tol || diff <= rel_tol * std::max(std::abs(a), std::abs(b));
}
```
