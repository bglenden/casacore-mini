# P12-W6: TaQL grouping/aggregate closure — Summary

## Objective
Replace GROUPBY/HAVING stubs with full implementation and add comprehensive
aggregate function support (GCOUNT, GSUM, GMIN, GMAX, GMEAN, GMEDIAN,
GVARIANCE, GSTDDEV, GRMS, GANY, GALL, GNTRUE, GNFALSE, GSUMSQR,
GPRODUCT, GFIRST, GLAST, GROWID).

## Changes

### src/taql.cpp
- Added `eval_aggregate()` (~130 lines): evaluates aggregate function calls
  over a set of rows, dispatching by function name.
- Added `contains_aggregate()` helper to detect aggregate calls in expression
  trees.
- Added `eval_with_aggregates()`: recursively resolves aggregates inside
  binary_op, comparison, logical_op, unary_op, and func_call expression nodes.
- Replaced GROUPBY/HAVING stubs with a real grouping engine:
  - Groups rows by evaluating group-key expressions.
  - Evaluates aggregate SELECT projections per group.
  - Filters groups via HAVING using `eval_with_aggregates`.
- Added ungrouped-aggregate path: if SELECT contains aggregates but no
  GROUPBY, all matching rows are treated as a single group.
- Changed multi-table overload error messages to distinguish from the
  now-removed single-table stubs.

### tests/taql_aggregate_test.cpp (new, 12 tests)
- Ungrouped: gcount, gsum, gmin/gmax, gmean, gmedian, gproduct.
- GROUPBY: gcount per group, gsum per group.
- HAVING: filter by aggregate sum, filter by count.
- Aggregate with WHERE pre-filter, multiple aggregates in one query.

### tests/taql_command_test.cpp (modified)
- Renamed `test_groupby_throws` → `test_groupby_executes` and
  `test_having_throws` → `test_having_executes` to verify correct execution
  instead of expecting runtime_error.

### CMakeLists.txt
- Added `taql_aggregate_test` target.

## Test results
- 98/98 tests pass (including 12 new aggregate tests).
- Wave gate: 8/8 checks pass.
