# P9-W8 Decisions

1. **Stress-test MS fixture**: 27-row MS with 6 antennas, 3 fields, 3 SPWs,
   3 scans, 3 states, and 2 array IDs provides full combinatorial coverage
   for parity assertions.

2. **Systematic row-count verification**: Each parity test computes expected
   row counts from the fixture structure (9 rows per scan, 9 per field, etc.)
   rather than hardcoding arbitrary numbers.

3. **Lambda-based malformed test**: `expect_throw` lambda captures the MS
   and tests each category's error path concisely without duplicating
   try/catch boilerplate.
