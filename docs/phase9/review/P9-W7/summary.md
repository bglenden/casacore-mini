# P9-W7 Summary: Selection Foundation

## Objective
Implement MsSelection class with expression parsing and evaluation for all
8 selection categories: antenna, field, SPW, scan, time, UV-distance,
correlation, and state.

## Deliverables

| File | Purpose |
|------|---------|
| `include/casacore_mini/ms_selection.hpp` | `MsSelection` class, `MsSelectionResult` struct |
| `src/ms_selection.cpp` | Expression parsing, row-set evaluation for all 8 categories |
| `tests/ms_selection_test.cpp` | 28 tests covering all categories + malformed input + AND logic |
| `tools/check_phase9_w7.sh` | Gate script |

## Test results
- 51/51 tests pass
- Zero compiler warnings
