# P12-W8: MSSelection parser/evaluator closure

## Objective
Close remaining MSSelection parser and evaluator gaps: regex/pattern matching,
antenna auto-correlation baselines, field negation/range/bounds, SPW name and
frequency selection, time-string parsing, UV unit handling, and state bounds.

## Changes

### src/ms_selection.cpp
- Added `#include <regex>` for regex pattern matching.
- Added helper functions:
  - `is_regex_pattern()` — detects regex metacharacters in a token.
  - `regex_match_str()` — regex match using `std::regex`.
  - `is_name_pattern()` — checks if token contains glob or regex patterns.
  - `name_match()` — unified pattern matching: regex > glob > exact match.
  - `parse_time_string()` — YYYY/MM/DD[/HH:MM:SS] to MJD seconds conversion
    using Fliegel & Van Flandern algorithm.
  - `parse_time_value()` — tries numeric double first, then date string.
  - `parse_uv_with_unit()` — strips unit suffix (m, km, wavelength, lambda, %)
    and returns value + multiplier.
  - `parse_chan_range()` — parses channel range with stride (e.g., "0~63^2").
- Antenna selection:
  - Fixed `&&` (with-auto) and `&&&` (auto-only) baseline semantics, mirroring
    the existing feed pair logic.
  - Changed `"0&"` to throw instead of acting as wildcard.
  - Updated name resolution to use `name_match()` for regex support.
- Field selection:
  - Added negation prefix `!` support.
  - Added `~` range syntax for numeric IDs.
  - Added `<`/`>` bound operators.
  - Updated name resolution to use `name_match()` for regex support.
- SPW selection:
  - Added ID range syntax `0~1`.
  - Added name-based selection via SPECTRAL_WINDOW subtable.
  - Added frequency-based selection with Hz/kHz/MHz/GHz suffixes.
  - Added frequency range with `~` (e.g., "1.0~2.0GHz").
  - Added channel stride with `^` (e.g., "0:0~63^2").
  - Restructured to check frequency suffix before numeric range to avoid
    false parse failures on expressions like "1.0~2.0GHz".
- Time selection:
  - Extended to accept date strings (YYYY/MM/DD, YYYY/MM/DD/HH:MM:SS) in
    addition to raw MJD second values.
  - Works with `>`, `<`, and `lo~hi` range operators.
- UV-distance selection:
  - Added unit suffix support: `m` (meters), `km` (kilometers).
  - Wavelength and percentage modes parsed but treated as raw values.
- State selection:
  - Added `<`/`>` bound operators.
  - Updated pattern matching to use `name_match()` for regex support.
- to_taql_where():
  - Updated antenna TaQL for `&&`/`&&&` semantics.
  - Updated field TaQL for negation and bounds.
  - Updated time TaQL to use `parse_time_value()` for date strings.
  - Updated state TaQL for bounds.
  - Updated antenna/field name resolution to use `name_match()`.
  - Removed unused `double_range_to_taql()` helper.

### tests/ms_selection_parity_extended_test.cpp (new, 33 checks)
- Antenna: with-auto (`&&`), auto-only (`&&&`), regex pattern.
- Field: negation, range, bounds, regex pattern.
- SPW: by name, range, by frequency, frequency range, glob, channel stride.
- Time: date string selection, date string range.
- UV-distance: km unit, m unit.
- State: bounds `>`, bounds `<`, regex pattern.

### CMakeLists.txt
- Added `ms_selection_parity_extended_test` target.

## Test results
- 100/100 tests pass (including 33 new parity-extended checks).
- Wave gate: 10/10 checks pass.
