# P12-W6 Open Issues

1. GROUPBY with multi-table queries (JOIN + GROUPBY) is not yet implemented.
   The multi-table overload throws a descriptive error if GROUPBY is attempted.
2. Aggregate functions over array columns (e.g., GSUM of an array column) are
   not handled; only scalar columns are supported.
3. GVARIANCE/GSTDDEV use population formulas (dividing by N), not sample
   formulas (dividing by N-1). This matches casacore behavior.
