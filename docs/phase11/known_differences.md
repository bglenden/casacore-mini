# Phase 11 Known Differences from Upstream casacore

This document lists all known behavioral differences between casacore-mini's
TaQL and MSSelection implementations and the upstream casacore equivalents.

## TaQL Differences

### D1: DDL commands (CREATE TABLE, ALTER TABLE, DROP TABLE)
Parser is complete. Executor is a no-op; DDL operations require filesystem
table creation/modification APIs not available in the TaQL executor path.
Table creation is available via `Table::create()` outside of TaQL.

### D2: INSERT/DELETE row mutations
Parser accepts both commands. Executor rejects with a clear error message.
INSERT requires `Table::addRow()` and DELETE requires `Table::removeRow()`,
neither of which exist in the Table API.

### D3: JOIN/multi-table operations
Parser handles JOIN ... ON, multiple JOINs, and multi-table FROM clauses.
Executor operates on a single table context. Multi-table query execution
would require a table registry and cross-table expression resolution.

### D4: GROUPBY/HAVING/aggregate functions
Parser handles GROUPBY, HAVING, ROLLUP, and all aggregate functions
(`gcount`, `gmin`, `gmax`, `gmean`, etc.). Executor does not implement
grouping/accumulation. Aggregate function calls throw at evaluation time.

### D5: Array column operations
Parser handles array indexing (`col[i]`, `col[i:j:k]`) and all array
functions (`ndim`, `nelem`, `shape`, `arrsum`, etc.). Evaluator only
handles scalar columns via `read_scalar_cell`. Array column access would
require `read_array_cell` integration.

### D6: Complex number arithmetic
Complex literals parse and store correctly. Binary operators use `as_double()`
which discards the imaginary part. Full complex arithmetic would require a
parallel evaluation path in `binary_op`.

### D7: Date/time functions
Parser handles all datetime function calls (`datetime`, `year`, `month`,
`day`, `mjdtodate`, etc.). Evaluation requires a date/time parsing and
conversion framework not currently available.

### D8: Angle functions
Parser handles angle functions (`hms`, `dms`, `hdms`, `normangle`,
`angdist`, `angdistx`). Evaluation requires a measures/angle conversion
framework.

### D9: Unit suffixes
Parser stores unit information on expression nodes. Evaluator does not
perform unit conversion (e.g., `1.5 DEG` evaluates as `1.5` without
converting to radians).

### D10: Cone search operations
Parser handles `INCONE`, `ANYCONE`, `FINDCONE` and related functions.
Evaluator requires spherical geometry implementation.

### D11: MeasUDF / UDF libraries
Not implemented. Requires a plugin/UDF registration architecture.

### D12: Subqueries and CTEs
Parser handles `EXISTS`, `FROM (subquery)`, and `WITH` CTEs. Executor
does not support recursive query execution or subquery evaluation.

### D13: LIKE pattern matching
Uses ECMAScript regex internally rather than POSIX regex. For standard
SQL patterns (`%`, `_`) and glob patterns (`*`, `?`), behavior is
equivalent. Exotic POSIX-only regex features may differ.

## MSSelection Differences

### D14: Antenna name regex
Antenna name lookup supports exact match and glob patterns (`DV*`, `ANT?`).
Upstream `MSAntennaGram` additionally supports regex (`/^DV[0-9]+$/`) on
antenna names, which is not implemented.

### D15: Date/time string parsing
Time selection only supports raw MJD double values. Upstream `MSTimeGram`
supports human-readable date/time formats (`YYYY/MM/DD/HH:MM:SS.sss`) and
wildcard time fields.

### D16: SPW frequency-based selection
SPW selection only supports numeric IDs and channel ranges. Upstream
`MSSpwGram` supports frequency ranges with units (`1.0GHz~2.0GHz`),
frequency with step (`freq^step`), and SPW name/pattern matching.

### D17: UV-distance units
UV-distance selection assumes meters. Upstream `MSUvDistGram` supports
unit specification (`m`, `km`, `wavelength`) and percentage variance syntax.

### D18: PARSE_LATE mode
All expressions are evaluated immediately (parse-now). Upstream supports
deferred parsing/evaluation mode.

### D19: Error handler infrastructure
Errors throw `std::runtime_error` immediately. Upstream `MSSelectionErrorHandler`
collects and reports multiple parse errors.

### D20: VisIter-style slice accessors
`getChanSlices()` and `getCorrSlices()` return VisIter-compatible slice objects
in upstream. casacore-mini provides `channel_ranges` and `correlations` vectors
instead.

### D21: Baseline pair matrix / split antenna lists
Upstream provides separate `getAntenna1List()`, `getAntenna2List()`, and
`getBaselineList()` (pair matrix). casacore-mini provides a flat `antennas`
vector. Similarly for feed1/feed2 split.

### D22: Correlation DDID mapping
Upstream maps correlation names to `DATA_DESC_ID` values via polarization
subtable lookup (`getDDIDList`, `getPolMap`, `getCorrMap`). casacore-mini
stores correlation names in `result.correlations` without DDID mapping.
