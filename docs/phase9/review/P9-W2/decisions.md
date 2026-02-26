# P9-W2 Decisions

## Autonomous decisions made

1. Used little-endian byte order for new MS files (matches x86/ARM64 platforms).
2. Routed main-table array columns (FLAG, UVW, SIGMA, WEIGHT, etc.) to TiledColumnStMan and scalar columns to StandardStMan.
3. Subtable schemas include all required columns from MS2 specification (full schemas to be expanded in W3).
4. MeasurementSet uses composition over TableDirectory rather than inheritance.

## Assumptions adopted

1. Subtable keyword values use "Table: <path>" format matching casacore convention.
2. table.info type string is "Measurement Set" (capital M, capital S).

## Tradeoffs accepted

1. Subtable .f0 files are created empty (0 rows) at creation time; data population deferred to W5 (MsWriter).
