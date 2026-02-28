# P11-W7 Open Issues

## O1: Array column evaluation
TaQL array functions (ndim, nelem, shape, arrsum, etc.) and array indexing (col[i]) parse correctly but the evaluator only handles scalar columns. Extending to array columns would require read_array_cell() integration in eval_expr.

## O2: GROUPBY/aggregate execution
Aggregate functions (gcount, gmin, gmax, gmean, etc.) and GROUPBY/HAVING parse but lack an execution engine. This would require a grouping/accumulation framework in the SELECT executor.

## O3: Multi-table operations
JOIN and multi-table FROM clauses parse but execute against a single table. Multi-table execution would require a table registry and cross-table expression resolution.

## O4: DDL execution
CREATE TABLE, ALTER TABLE, DROP TABLE parse completely but don't execute. These require Table creation/modification/deletion APIs that don't currently exist in the TaQL executor path.

## O5: INSERT/DELETE row mutations
INSERT parses but Table API lacks addRow(). DELETE identifies matching rows but Table API lacks removeRow(). Both return metadata about intended operations.

## O6: Complex number arithmetic
Complex literals parse and store correctly, but binary_op uses as_double() which discards the imaginary part. Full complex arithmetic would require a parallel evaluation path.

## O7: Date/time and angle functions
Parser handles all datetime and angle function calls. Evaluation requires a date/time parsing framework and measures/unit conversion system not currently available.

## O8: MSSelection name glob/regex
Antenna and field name matching supports exact match and simple glob (field only). Full glob patterns on antenna names and regex matching require enhanced string pattern matching in the subtable lookup code.
