# P9-W5 Summary: Write/Update Flows and Persistent Integrity

## Objective
Implement MsWriter for batch-populating a MeasurementSet with main-table rows
and subtable data, with foreign-key validation.

## Deliverables

| File | Purpose |
|------|---------|
| `include/casacore_mini/ms_writer.hpp` | MsWriter class + row structs (MsMainRow, MsAntennaRow, etc.) |
| `src/ms_writer.cpp` | Batch write: collect rows, validate FKs, flush to SSM/TSM |
| `tests/ms_writer_test.cpp` | 5 tests: write+reread, FK invalid, FK valid, empty write, multiple subtable rows |
| `tools/check_phase9_w5.sh` | Gate script |

## Design
- Batch-write model: rows collected in memory, flushed all at once (matches SM writer constraints)
- FK validation: checks ANTENNA1/2, FIELD_ID, DATA_DESC_ID, OBSERVATION_ID, STATE_ID
- Subtable writes via reusable `flush_subtable()` helper with column-name dispatch lambdas
- TSM array columns forced to uniform [3] shape for TiledColumnStMan compatibility

## Test results
- 48/48 tests pass (44 existing + 4 new MS tests)
- Zero compiler warnings
