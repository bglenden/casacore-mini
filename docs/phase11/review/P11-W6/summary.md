# P11-W6 Review Summary: MS selection evaluator + to_table_expr_node bridge

## Deliverables

| Artifact | Status |
|---|---|
| `include/casacore_mini/ms_selection.hpp` | Expanded — `to_taql_where()`, 12 expression accessors |
| `src/ms_selection.cpp` | Complete — WHERE clause generation for all 12 categories |
| `tests/ms_selection_table_expr_bridge_test.cpp` | 28 checks: WHERE generation + TaQL execution parity |
| `tools/check_phase11_w6.sh` | 25/25 gate checks pass |

## Changes

### to_taql_where() bridge method
- Converts MSSelection category expressions to a TaQL WHERE clause string
- Equivalent to casacore's `MSSelection::toTableExprNode()` but produces a string
- The returned string can be used with `taql_execute("SELECT FROM t WHERE " + expr, table)`
- Returns empty string when no selection is set

### Helper functions (internal)
- `int_expr_to_taql(col, expr)` — converts comma-separated IDs and ranges to `col IN [...]`
- `bound_to_taql(col, expr)` — converts `<N` / `>N` bound syntax
- `double_range_to_taql(col, expr)` — converts float range syntax for time/uvdist

### Expression accessors (12 const getters)
- Each returns `const std::optional<std::string>&` for the stored expression
- Covers all 12 categories: antenna, field, spw, scan, time, uvdist, corr, state, observation, array, feed, taql

### Category-to-WHERE mapping
| Category | WHERE clause pattern |
|---|---|
| antenna | `(ANTENNA1 IN [...] OR ANTENNA2 IN [...])` |
| field | `FIELD_ID IN [...]` |
| spw | `DATA_DESC_ID IN [...]` |
| scan | `SCAN_NUMBER IN [...]` or `SCAN_NUMBER > N` |
| time | `TIME > N` or `(TIME >= N AND TIME <= M)` |
| uvdist | `SQRT(SUMSQR(UVW[0],UVW[1])) > N` |
| state | `STATE_ID IN [...]` |
| observation | `OBSERVATION_ID IN [...]` or bounds |
| array | `ARRAY_ID IN [...]` or bounds |
| feed | `(FEED1 IN [...] OR FEED2 IN [...])` |
| taql | raw expression injected directly |

## Test coverage

- Empty selection → empty WHERE string
- Single-category WHERE: scan, field, observation, array, antenna, state
- Combined selection → AND-joined WHERE
- Scan bounds → `SCAN_NUMBER > 2` syntax
- TaQL injection → raw expression appears in WHERE
- Expression accessors: set/get/clear round-trip
- **Parity verification**: each test runs both `to_taql_where()` via TaQL and `evaluate()` directly, then verifies identical row counts

## Build & CI

- 7 MS selection + TaQL test suites all pass (28 + 28 + 13 + 28 + 10 + 51 + 24 = 182 total checks)
- clang-tidy clean (3 NOLINT for unavoidable branch-clone in bound handling)
- No regressions in any prior test suite
