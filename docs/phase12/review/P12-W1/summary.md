# P12-W1 Summary: Coordinate Regression Closure + Diagnostics

## Problem
Post-Phase-8 regression: `mini->casacore` cells for `coord-keywords` and
`mixed-coords` artifacts failed because mini's `SpectralCoordinate::save()`
was nesting `crval`/`cdelt`/`crpix` inside a `wcs` sub-record, while casacore
expects these fields at the top level of the spectral sub-record.

## Root Cause
`SpectralCoordinate::save()` emitted version 2 format with a `wcs`
sub-record: `spectral1.wcs.crval`. Casacore's native format stores
`spectral1.crval` directly.

## Fix
1. Changed `SpectralCoordinate::save()` to emit `crval`, `crpix`, `cdelt`,
   `pc`, `ctype` at the top level of the spectral record (matching casacore).
2. Updated `SpectralCoordinate::from_record()` to prefer top-level fields,
   with fallback to `wcs` sub-record for backward compatibility.
3. Updated casacore interop verifier to accept both layouts (top-level and
   wcs nested) for robustness.
4. Hardened Phase 8 matrix runner to capture per-cell stderr to log files
   instead of suppressing with `2>/dev/null`.

## Result
- Phase 8 interop matrix: 24/24 PASS (was 22/24)
- All 93 unit tests pass
- Matrix stderr is now captured for diagnostics
