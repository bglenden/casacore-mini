# P9-W10 Summary: Mutation Operations

## Objective
Implement MsConcat (concatenate MS trees with subtable merging and ID remapping)
and MsFlagger (flag/unflag rows by modifying FLAG_ROW in SSM).

## Deliverables

| File | Purpose |
|------|---------|
| `include/casacore_mini/ms_concat.hpp` | `ms_concat()` free function, `MsConcatResult` |
| `include/casacore_mini/ms_flagger.hpp` | `ms_flag_stats()`, `ms_flag_rows()`, `ms_unflag_rows()` |
| `src/ms_concat.cpp` | Concat with antenna/field/SPW/DD/state dedup + ID remapping |
| `src/ms_flagger.cpp` | Read-modify-write SSM for FLAG_ROW column |
| `tests/ms_concat_test.cpp` | 4 tests: row count, antenna dedup, field dedup, ID remapping |
| `tests/ms_flagger_test.cpp` | 5 tests: stats, flag, unflag, data preservation, empty ops |
| `tools/check_phase9_w10.sh` | Gate script |

## Test results
- 56/56 tests pass
- Zero compiler warnings
