# P12-W7 Decisions

1. MeasUDF equivalents use `MEAS_DIR_J2000(lon, lat)` naming convention with
   underscores instead of dots, bridging to `convert_measure()`. This avoids
   parser changes while providing equivalent functionality.
2. Complex-aware functions (REAL, IMAG, NORM, ARG, CONJ) check for
   `complex<double>` variant before `as_double()` to avoid type errors when
   nesting complex expressions.
3. Unit suffix evaluation uses a simple multiplication factor from
   `unit_to_si()`, applied via the `apply_unit` lambda in `eval_expr()`.
4. DateTime functions use MJD (Modified Julian Date) as the native time
   representation, matching casacore's convention. The Fliegel & Van Flandern
   algorithm converts between MJD and Gregorian dates.
5. Array reduction functions (ARRSUM, ARRMIN, etc.) accept both scalar and
   vector arguments; scalars are treated as single-element vectors.
