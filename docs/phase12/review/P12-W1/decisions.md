# P12-W1 Decisions

1. **SpectralCoordinate record layout**: Emit crval/cdelt/crpix at top level
   of the spectral sub-record (matching casacore), not nested under `wcs`.
   The `from_record` reader accepts both formats for backward compatibility.

2. **Verifier robustness**: The casacore interop verifier now accepts both
   top-level and wcs-nested spectral fields, rather than requiring exact
   casacore layout.

3. **Stderr diagnostics**: Matrix runner captures per-cell stderr to log
   files rather than discarding, enabling diagnostic visibility on failures.
