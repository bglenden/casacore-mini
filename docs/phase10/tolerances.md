# Phase 10 Numeric Tolerance Policy

Inherits Phase 9 base tolerances. Additions below are specific to
image/lattice/expression workflows.

## Pixel value tolerances

### Float pixels (PagedImage<Float>, ArrayLattice<Float>)
- Absolute tolerance: 1e-5
- Relative tolerance: 1e-6
- Rationale: float32 has ~7 significant digits.

### Double pixels (ArrayLattice<Double>)
- Absolute tolerance: 1e-10
- Relative tolerance: 1e-12

### Complex pixels (PagedImage<Complex>)
- Apply float tolerance independently to real and imaginary parts.

## Expression evaluation tolerances

### Arithmetic expressions
- Same as pixel tolerances for the result type.

### Transcendental functions (sin, cos, exp, log, sqrt, etc.)
- Absolute tolerance: 2e-5 (float), 2e-10 (double)
- Rationale: platform libm differences may introduce 1-2 ULP variation.

### Reductions (sum, mean, variance, stddev, median)
- Absolute tolerance: N * 1e-5 (float) where N is element count
- Relative tolerance: 1e-4 (float), 1e-10 (double)
- Rationale: accumulation order may differ; larger arrays accumulate more error.

## Coordinate tolerances

Inherited from Phase 8/9:
- Direction values (RA/Dec): 1e-12 rad
- Frequency values: 1e-10 Hz (absolute)
- Reference pixel positions: exact match

## Shape and mask tolerances

- Array shapes: exact match
- Mask values (Bool): exact match
- Region parameters (box corners, polygon vertices): 1e-10 (double coords)

## Comparison implementation

Use the same `approx_equal` function from Phase 9 tolerances.
