# P11-W7 Review Summary: TaQL + MS selection parity closure

## Deliverables

| Artifact | Status |
|---|---|
| `src/taql.cpp` | Extended — 12 new evaluator functions, regex_expr, LIKE improvement |
| `tests/taql_w7_closure_test.cpp` | 32 checks: string funcs, predicates, LIKE, rownr, end-to-end |
| `docs/phase11/taql_command_checklist.csv` | Updated — 49 Done, 0 Pending, remainder Simplified/Excluded |
| `docs/phase11/msselection_capability_checklist.csv` | Updated — 59 Done, 0 Pending, remainder Simplified |
| `tools/check_phase11_w7.sh` | 29/29 gate checks pass |

## TaQL evaluator extensions

### New string functions
- `LTRIM`, `RTRIM` — leading/trailing whitespace removal
- `CAPITALIZE` — capitalize first letter of each word
- `SREVERSE` — reverse string
- `SUBSTR(str, start [, len])` — substring extraction
- `REPLACE(str, old, new)` — string replacement

### New predicate functions
- `ISCOLUMN(name)` — check if column exists in table
- `ISKEYWORD(name)` — check if keyword exists in table
- `ISDEF(val)` — check if value is defined (always true for scalars)
- `ISNULL(val)` — check if value is null/monostate

### Enhanced functions
- `NEAR(a, b, tol)` / `NEARABS(a, b, tol)` — 3-argument tolerance form
- `ROWNR()` / `ROWID()` — returns actual row number from evaluation context
- `RAND()` — returns random double in [0, 1)
- `BOOL(x)` — type conversion to boolean
- `STRING(x)` — type conversion to string

### New evaluator expression types
- `regex_expr` — `~` and `!~` regex matching via C++ `<regex>` (ECMAScript)
- `like_expr` — improved to full SQL/glob pattern matching via regex conversion (supports `%`, `_`, `*`, `?` anywhere in pattern)

## Checklist status closure

### TaQL command checklist (117 rows)
- **49 Done**: full parser + evaluator implementation with test coverage
- **56 Simplified**: parser complete, evaluator limited (DDL, JOIN, GROUPBY, arrays, datetime, etc.)
- **2 Excluded**: UDF libraries and MeasUDF (require plugin architecture beyond scope)
- **0 Pending**

### MSSelection capability checklist (95 rows)
- **59 Done**: full parser + evaluator + accessor implementation with test coverage
- **36 Simplified**: parser handles syntax but evaluator limited (regex names, frequency selection, VisIter slices, etc.)
- **0 Pending**

## End-to-end integration tests

- TaQL query over MS main table using MSSelection WHERE clause
- MSSelection with raw TaQL injection combined with category selection
- TaQL COUNT over MS tables
- Parity verification: TaQL path matches evaluate() path

## Build & CI

- 8 test suites all pass (32 + 51 + 24 + 28 + 28 + 13 + existing ms_selection tests)
- clang-tidy clean (0 user-code warnings)
- No regressions in any prior test suite
