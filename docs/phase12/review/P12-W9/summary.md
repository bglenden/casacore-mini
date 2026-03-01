# P12-W9 Summary: MSSelection API/accessor/mode/error closure

## What was done

1. **Error-collection mode**: Added `ErrorMode::CollectErrors` that wraps each
   selection category in `try { ... } catch` and accumulates errors in
   `MsSelectionResult::errors` instead of throwing. All 12 categories
   (antenna, field, SPW, scan, time, UV, correlation, state, observation,
   array, feed, TaQL) are wrapped.

2. **ParseMode support**: Added `ParseMode::ParseNow` / `ParseLate` enum and
   `set_parse_mode()` / `parse_mode()` accessors. Both modes are functionally
   equivalent (expressions are always validated at `evaluate()` time); the
   distinction is API-compatible with upstream casacore.

3. **Split antenna/feed lists**: Baseline pair expressions (`"0&1"`, `"0&&&"`)
   now populate `antenna1_list` / `antenna2_list` and `feed1_list` /
   `feed2_list` in addition to the combined `antennas` / `feeds` vectors.

4. **Time/UV range outputs**: `MsSelectionResult::time_ranges` stores parsed
   `TimeRange` structs (lo, hi, is_seconds flag). `uv_ranges` stores parsed
   `UvRange` structs (lo, hi, unit enum).

5. **DDID/polarization maps**: SPW selection now populates `data_desc_ids`,
   `ddid_to_spw` (DDID → SPW_ID map), and `ddid_to_pol_id` (DDID →
   POLARIZATION_ID map).

6. **Mode reset**: `clear()` and `reset()` now reset `parse_mode_` and
   `error_mode_` to defaults.

7. **TaQL bridge fix**: `to_taql_where()` state pattern matching now uses
   `name_match()` (regex+glob) instead of `glob_match()` only.

## Tests

- New `tests/ms_selection_api_parity_test.cpp`: 52 checks covering all new
  API surfaces.
- All 101 existing tests continue to pass.
