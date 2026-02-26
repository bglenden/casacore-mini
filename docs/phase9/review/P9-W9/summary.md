# P9-W9 Summary: Read/Introspection Operations

## Objective
Implement MsMetaData (cached metadata queries) and MsSummary (text summary
generation) for MeasurementSet introspection.

## Deliverables

| File | Purpose |
|------|---------|
| `include/casacore_mini/ms_metadata.hpp` | `MsMetaData` class with cached antenna/field/spw/obs/scan queries |
| `include/casacore_mini/ms_summary.hpp` | `ms_summary()` free function |
| `src/ms_metadata.cpp` | Lazy-cached metadata loading via column wrappers |
| `src/ms_summary.cpp` | Text summary generation using MsMetaData |
| `tests/ms_metadata_test.cpp` | 7 tests: all subtable queries, aggregates, empty MS, cache |
| `tests/ms_summary_test.cpp` | 3 tests: required sections, empty MS, multi-field |
| `tools/check_phase9_w9.sh` | Gate script |

## Test results
- 54/54 tests pass
- Zero compiler warnings
