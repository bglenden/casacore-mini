# P9-W3 Decisions

## Autonomous decisions made

1. Used `make_*_desc()` free functions returning `vector<ColumnDesc>` rather than per-subtable classes, matching the architectural decision from W1.
2. All subtable columns use StandardStMan as the default storage manager (appropriate for scalar-dominated subtables).
3. Optional subtable schemas (DOPPLER, FREQ_OFFSET, SOURCE, SYSCAL, WEATHER) are fully defined, not minimal stubs.

## Assumptions adopted

1. Column names and types match MS2 specification from casacore upstream headers.
2. Array column shapes follow casacore conventions (e.g., POSITION=[3], DIRECTION=[2], TIME_RANGE=[2]).

## Tradeoffs accepted

None.
