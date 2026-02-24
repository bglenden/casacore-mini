# P7-W12 Summary

## Scope implemented

StandardStMan (SSM) read path: bucket/file parser, row/cell lookup for scalar
columns (Int, Float, Double, String) and fixed-shape array columns (Float, Int).

## Public API changes

- New `SsmReader` class in `include/casacore_mini/standard_stman.hpp`
- `CellValue` variant extended with array types for typed cell retrieval

## Behavioral changes

1. `interop_mini_tool verify-table-dir`: now performs cell-value verification
   using SsmReader when SSM data files are present.
2. Matrix `casacore->mini` cell for `table_dir` now passes with value-level
   parity.

## Deferred items

SSM write path deferred to W13.
