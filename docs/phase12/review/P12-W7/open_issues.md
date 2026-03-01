# P12-W7 Open Issues

1. MeasUDF functions use underscore naming (`MEAS_DIR_J2000`) rather than the
   casacore dot convention (`meas.dir.j2000`) because the TaQL parser treats
   dotted identifiers as qualified column references. A parser extension to
   support dotted function names could be added if needed.
2. Array functions operate on 1D vectors only; multi-dimensional array
   semantics (TRANSPOSE, DIAGONAL) are simplified to 1D operations.
3. DATETIME parser handles `YYYY/MM/DD` and `YYYY/MM/DD/HH:MM:SS` formats
   only. ISO 8601 and other formats are not supported.
4. Unit suffix conversion is basic multiplication to SI. No compound unit
   parsing (e.g., `km/s` as a single suffix) is supported.
5. Cone search functions use simplified 5-arg form. The casacore INCONE with
   array-of-cones is not yet supported.
