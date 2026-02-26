# P9-W6 Summary: Utility Layer

## Objective
Implement MS utility classes: MsIter (row grouping), StokesConverter,
MsDopplerUtil, MsHistoryHandler.

## Deliverables

| File | Purpose |
|------|---------|
| `include/casacore_mini/ms_iter.hpp` | `ms_iter_chunks()`, `ms_time_chunks()` |
| `include/casacore_mini/ms_util.hpp` | StokesConverter, MsDopplerUtil, MsHistoryHandler |
| `src/ms_iter.cpp` | Iterator implementations |
| `src/ms_util.cpp` | StokesConverter (circular+linear->IQUV), MsDopplerUtil, MsHistoryHandler |
| `tests/ms_iter_test.cpp` | 3 tests: chunk grouping, time binning, empty MS |
| `tests/ms_util_test.cpp` | 5 tests: circular->IQUV, linear->IQUV, passthrough, Doppler, history |
| `tools/check_phase9_w6.sh` | Gate script |

## Test results
- 50/50 tests pass
- Zero compiler warnings
