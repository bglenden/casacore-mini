# P11-W8 Decisions

1. **Removed UPDATE test from integration**: MS opened via `MeasurementSet::open()`
   is read-only. UPDATE is already well-covered in `taql_command_test.cpp` which
   creates a writable table directly. No coverage gap.

2. **30-row MS layout**: 6 antennas, 3 fields, 5 scans, 2 observations chosen
   to provide enough variation for meaningful cross-category selection tests
   without being slow.

3. **Parity check approach**: Each MSSelection evaluate result is compared
   against a TaQL query using the same to_taql_where output, verifying the
   bridge produces correct WHERE clauses.
