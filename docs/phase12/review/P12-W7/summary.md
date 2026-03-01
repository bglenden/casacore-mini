# P12-W7: TaQL expression-function closure + built-in MeasUDF equivalents

## Objective
Close all remaining TaQL expression and function evaluation gaps: datetime,
angle, array, complex, unit suffix, cone-search, pattern, and built-in
measure conversion functions.

## Changes

### src/taql.cpp
- Added datetime helpers: `mjd_to_broken()`, `month_name()`, `dow_name()`,
  `iso_week()` for MJD-based date/time decomposition.
- Added angle helpers: `rad_to_hms()`, `rad_to_dms()`, `norm_angle()`,
  `ang_dist()` (Vincenty formula).
- Added unit conversion: `unit_to_si()` supporting 35+ unit names
  (angle, time, length, frequency, flux, temperature, velocity).
- Added array value helpers: `as_double_vec()`, `value_nelem()`, `value_ndim()`.
- Extended `eval_math_func()` with 60+ new function handlers:
  - **DateTime**: YEAR, MONTH, DAY, CMONTH, WEEKDAY, CDOW, WEEK, TIME,
    DATE, MJD, MJDTODATE, CTOD, CDATE, CTIME, DATETIME (string parse).
  - **Angle**: HMS, DMS, HDMS, NORMANGLE, ANGDIST (4-arg), ANGDISTX.
  - **Array info**: NDIM, NELEM, SHAPE.
  - **Array reduction**: ARRSUM, ARRMIN, ARRMAX, ARRMEAN, ARRMEDIAN,
    ARRVARIANCE, ARRSTDDEV, ARRRMS, ARRAVDEV, ARRANY, ARRALL, ARRNTRUE,
    ARRNFALSE, ARRFLAT, AREVERSE, ARRFRACTILE.
  - **Array construction**: ARRAY, TRANSPOSE, DIAGONAL, RESIZE.
  - **Complex**: COMPLEX (constructor), CONJ, REAL, IMAG, NORM, ARG
    (all complex-aware, check variant before calling `as_double`).
  - **Pattern**: REGEX, PATTERN, SQLPATTERN.
  - **Cone search**: INCONE, ANYCONE, FINDCONE (function form).
  - **MeasUDF equivalents**: MEAS_DIR_<ref>, MEAS_EPOCH_<ref>, MEAS_POS_<ref>
    (bridge to `convert_measure()`).
- Added `unit_expr` handler in `eval_expr()`: applies `unit_to_si()` factor.
- Added `subscript` handler in `eval_expr()`: array indexing for vector types.
- Added `cone_expr` handler in `eval_expr()`: INCONE expression form.
- Added `apply_unit` lambda in `eval_expr()` for unit suffix on literals.
- Restructured 1-arg function dispatch to check complex/array/string functions
  BEFORE `as_double()` to avoid type errors.

### tests/taql_udf_equiv_test.cpp (new, 65 checks)
- DateTime: year, month, day, cmonth, weekday, cdow, week, mjdtodate,
  datetime parse, date, time.
- Angle: hms, dms, normangle, angdist.
- Array: ndim, nelem, array create, arrsum, arrmin/max, arrmean, arrmedian.
- Complex: constructor, norm, arg, conj, real, imag.
- Unit suffix: deg→rad, arcsec→rad, km→m.
- Cone search: incone, findcone.
- MeasUDF: dir j2000 identity, dir galactic, epoch tai.
- Pattern: pattern, regex.

### CMakeLists.txt
- Added `taql_udf_equiv_test` target.

## Test results
- 99/99 tests pass (including 65 new UDF-equiv checks).
- Wave gate: 9/9 checks pass.
