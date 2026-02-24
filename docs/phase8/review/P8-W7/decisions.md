# P8-W7 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **WCSLIB-backed DirectionCoordinate** — DirectionCoordinate delegates all
   pixel<->celestial transforms to WCSLIB's `wcss2p`/`wcsp2s` functions. This
   provides correct handling of all FITS-standard projection types (SIN, TAN,
   ARC, SFL, etc.) with proper handling of celestial pole offsets, longitude
   wrapping, and coordinate system rotations.

2. **Lookup-table SpectralCoordinate** — SpectralCoordinate supports both
   linear mode (for regularly-spaced frequency axes) and lookup-table mode
   (for irregularly-spaced axes). Lookup-table mode uses linear interpolation
   between tabulated values, matching casacore's `TabularCoordinate`-backed
   spectral axis behavior.

## Tradeoffs accepted

1. WCSLIB-backed DirectionCoordinate means coordinate construction requires
   populating a WCSLIB `wcsprm` struct, which is somewhat verbose. Accepted
   because the alternative (reimplementing projection math) would be far
   more complex and error-prone.

2. Lookup-table SpectralCoordinate uses linear interpolation, which may not
   perfectly represent non-linear spectral axes. This matches casacore's
   behavior and is sufficient for the channel spacings found in practice.
