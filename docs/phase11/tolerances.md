# Phase 11 Tolerances

Date: 2026-02-27

## Purpose

Documents acceptable tolerances for interoperability comparisons.

## Numeric tolerances

| Type | Tolerance | Context |
|------|-----------|---------|
| Double | 1e-15 (relative) | Scalar values, array elements |
| Float | 1e-7 (relative) | Single-precision values |
| Complex | 1e-15 per component | Real and imaginary parts |
| Integer | exact | No tolerance |
| Boolean | exact | No tolerance |
| String | exact | Character-for-character |

## Datetime tolerances

| Context | Tolerance |
|---------|-----------|
| MJD values | 1e-10 days (~8.6 microseconds) |
| Epoch conversions | 1e-8 days (~0.86 milliseconds) |

## Coordinate tolerances

| Context | Tolerance |
|---------|-----------|
| Direction (ra/dec) | 5e-6 radians (~1 arcsec) |
| Frequency | 1e-10 (relative) |
| Position (ITRF) | 1e-3 meters |

## TaQL-specific tolerances

| Context | Tolerance |
|---------|-----------|
| Row selection results | exact (same row set) |
| Aggregate functions | 1e-12 (relative) |
| Expression evaluation | matches type-specific tolerance above |
| CALC output | matches type-specific tolerance above |

## MSSelection-specific tolerances

| Context | Tolerance |
|---------|-----------|
| Selected rows | exact (same row set) |
| Selected IDs | exact (same ID set) |
| Channel ranges | exact |
| Time ranges | 1e-10 days |
| UV distance ranges | 1e-10 (relative) |
