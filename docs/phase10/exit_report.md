# Phase 10 Exit Report

## Objective

Full Lattices + Images implementation, including lattice expression language
(LEL) compatibility, image metadata lifecycle, region/mask persistence,
and strict binary-level interoperability with casacore.

## Outcome: Complete

All 12 waves (W1-W12) completed. 78 tests pass. 20/20 interop matrix cells
PASS across 5 image artifacts x 4 directions.

## Wave Summary

| Wave | Scope | Status |
|------|-------|--------|
| W1 | API/corpus/LEL contract freeze + dependency lock | Done |
| W2 | Lattice storage/view substrate | Done |
| W3 | Lattice core model and traversal | Done |
| W4 | Image core models and metadata lifecycle | Done |
| W5 | Region and mask persistence | Done |
| W6 | Expression engine foundation | Done |
| W7 | LEL parser and operator/function coverage | Done |
| W8 | Mutation flows and persistence integrity | Done |
| W9 | Utility-layer parity and integration cleanup | Done |
| W10 | Strict image/lattice interoperability matrix | Done |
| W11 | Hardening + corpus expansion + docs convergence | Done |
| W12 | Closeout and Phase-11 handoff | Done |

## Key Deliverables

### Lattice Layer
- `LatticeArray<T>`: COW flat-vector storage with Fortran-order mdspan views
- `PagedArray<T>`: disk-backed lattice via Table/TSM round-trip
- `SubLattice<T>`, `LatticeConcat<T>`, `RebinLattice<T>`, `TempLattice<T>`
- `LatticeIterator<T>`: chunk-based traversal (collapsed from casacore's
  LatticeNavigator/Stepper hierarchy)

### LEL (Lattice Expression Language)
- Full expression tree: `LelNode<T>`, binary/unary/function/reduction nodes
- Type-erased wrapper: `LatticeExprNode`
- String parser: `lel_parse()` supporting arithmetic, comparison, logical,
  math functions (sin, cos, sqrt, log, log10, exp, pow, etc.), reductions
  (sum, mean, min, max, median, variance, stddev, ntrue, nfalse, any, all),
  `iif()`, `value()`, `mask()`
- 8-category test coverage per `lel_coverage_contract.md`

### Image Layer
- `ImageInterface<T>`: abstract base with coordinate system, units, info
- `PagedImage<T>`: disk-backed image with TSM storage
- `SubImage<T>`: slicer-based region view
- `ImageExpr<T>`: expression-result wrapper (read-only)
- `ImageConcat<T>`: multi-image concatenation (read-only)
- `ImageInfo`, `ImageBeamSet`, `MaskSpecifier`, `ImageSummary`
- `image_utilities::statistics`, `copy_image`, `scale_image`
- `image_regrid()`: nearest-neighbor reprojection

### Region Layer
- LC regions: `LcBox`, `LcPixelSet`, `LcPolygon`, `LcEllipsoid`, `LcMask`,
  `LcIntersection`, `LcUnion`, `LcDifference`, `LcComplement`, `LcExtension`
- WC regions: `WcBox`, `WcPolygon`, `WcEllipsoid`,
  `WcIntersection`, `WcUnion`, `WcDifference`, `WcComplement`
- `ImageRegion`, `RegionManager`, `RegionHandler`

### Interoperability Matrix (20/20 PASS)

| Artifact | casa->casa | mini->mini | casa->mini | mini->casa |
|----------|-----------|-----------|-----------|-----------|
| img-minimal | PASS | PASS | PASS | PASS |
| img-cube-masked | PASS | PASS | PASS | PASS |
| img-region-subset | PASS | PASS | PASS | PASS |
| img-expression | PASS | PASS | PASS | PASS |
| img-complex | PASS | PASS | PASS | PASS |

Key interop fixes during Phase 10:
- Bool bit-packing in TSM data files (1 bit per element, LSB first)
- TiledShapeStMan/TiledCellStMan/TiledDataStMan tile-size calculations
  corrected for Bool columns

## Test Coverage

- 78 total tests (unit tests + demos + hardening)
- 2 new hardening test suites: `image_malformed_test` (23 assertions),
  `lel_malformed_test` (47 assertions)
- Parser-level coverage added for `median()`, `mean()`, `log10()`

## Known Differences from Casacore

1. `ImageExpr::has_pixel_mask()` returns `false` unconditionally; masks from
   LEL expression results are not surfaced through the ImageInterface API.
2. `ImageConcat` reads pixel data eagerly (not lazily).
3. `image_regrid` fills out-of-bounds pixels with zero (casacore masks them).
4. Traversal classes (LatticeNavigator, LatticeStepper, etc.) collapsed into
   a single `LatticeIterator<T>`.

## Documentation Convergence (W11)

- `api_surface_map.csv`: all 65 entries updated from `pending` to `done`;
  stale header references and non-existent class mappings corrected.
- `lel_parse` Doxygen updated to list `value(expr)`, `mask(expr)`, `median()`.
- `ImageConcat` Doxygen corrected from "lazily" to "eagerly".

## Phase-11 Entry Conditions

Phase 11 is the planned terminal phase. Entry conditions:
1. Phase 10 exit report accepted (this document)
2. All 78 tests passing
3. 20/20 interop matrix cells PASS
4. First W11-1 task: full missing-capabilities audit across all modules
