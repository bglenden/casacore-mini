# P9-W11 Decisions

1. **No optional subtables in ms-optional-subtables artifact**: casacore-mini
   does not implement WEATHER, SOURCE, POINTING, SYSCAL, FREQ_OFFSET, or
   DOPPLER subtables. The ms-optional-subtables artifact verifies only
   the 12 required subtables are present.

2. **ms-concat artifact uses directory layout**: The produce command creates
   a directory containing ms_a.ms, ms_b.ms, and concat.ms. The verify
   command opens concat.ms inside the input directory.

3. **No DATA column in artifacts**: All artifacts use scalar columns +
   UVW/SIGMA/WEIGHT arrays only. The DATA column (complex visibility)
   requires TiledStMan and is not included in interop artifacts.

4. **Hardening tests use std::filesystem temp dirs**: All malformed tests
   create temporary directories and clean up after each test.

5. **Cross-tool verification tolerant of subtable details**: Verify functions
   check structural properties (row counts, antenna/field/SPW counts,
   scan/array_id sets) rather than exact cell values, allowing both tools
   to produce semantically equivalent but not byte-identical artifacts.
