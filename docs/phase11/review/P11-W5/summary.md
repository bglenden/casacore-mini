# P11-W5 Review Summary: MS selection grammar/parser parity

## Deliverables

| Artifact | Status |
|---|---|
| `include/casacore_mini/ms_selection.hpp` | Expanded — 12 category setters, clear/reset, 3 new result fields |
| `src/ms_selection.cpp` | Complete — observation, array, feed, TaQL injection parsers; scan bounds |
| `tests/ms_selection_parser_test.cpp` | 28 checks: obs/arr/feed/taql/scan-bounds/clear/combined |
| `tests/ms_selection_malformed_test.cpp` | 13 checks: error paths for all new categories |
| `tools/check_phase11_w5.sh` | 36/36 gate checks pass |

## Changes

### New category setters (4)
- `set_observation_expr()` — filter by OBSERVATION_ID (ID, range, bounds `< >`)
- `set_array_expr()` — filter by ARRAY_ID (ID, range, bounds `< >`)
- `set_feed_expr()` — filter by FEED1/FEED2 (ID, range, pair cross `&`, with-auto `&&`, auto-only `&&&`, negation `!`)
- `set_taql_expr()` — raw TaQL WHERE clause injection via `taql_execute()`

### Enhanced existing categories
- Scan: added `< >` bound operators (was only ID/range/negate)

### API additions
- `clear()` — reset all 12 expression fields
- `reset()` — alias for clear
- `MsSelectionResult::observations` — selected observation IDs
- `MsSelectionResult::arrays` — selected (sub)array IDs
- `MsSelectionResult::feeds` — selected feed IDs

## Test coverage

- Observation: by ID, range, bounds `< >`, combined
- Array: by ID, bounds
- Feed: by ID, cross pair, with-auto pair, auto-only, negation
- TaQL injection: standalone, combined with observation
- Scan bounds: `< >` operators
- Clear/reset: verify selection cleared and no-op evaluation
- Combined: observation AND array intersection
- Malformed: empty, non-numeric, incomplete for each new category

## Build & CI

- 4 MS selection test suites all pass (28 + 13 + existing 28 + existing 10)
- clang-tidy clean
- No regressions in existing tests
