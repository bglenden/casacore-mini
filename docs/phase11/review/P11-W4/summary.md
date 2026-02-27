# P11-W4 Review Summary: TaQL evaluator + command execution

## Deliverables

| Artifact | Status |
|---|---|
| `src/taql.cpp` (evaluator) | Complete — recursive expr evaluator, 30+ built-in functions, 6 command handlers |
| `tests/taql_eval_test.cpp` | 51 checks: arithmetic, comparisons, logic, math functions, constants, IIF, strings |
| `tests/taql_command_test.cpp` | 24 checks: SELECT (all/where/orderby/limit/projection), COUNT, CALC, UPDATE, DELETE, SHOW, column expressions |
| `tools/check_phase11_w4.sh` | 65/65 gate checks pass |

## Architecture

- **eval_expr()**: Recursive evaluator handling 14+ expression node types: literal, column_ref, unary_op, binary_op, comparison, logical_op, func_call, iif_expr, in_expr, between_expr, like_expr, set_expr, wildcard, keyword_ref.
- **eval_math_func()**: 30+ built-in functions — trig (sin/cos/tan/asin/acos/atan), exp/log, rounding (floor/ceil/round), abs, pow, sqrt, min/max, square/cube, isnan/isinf/isfinite, string functions (strlen/upcase/downcase/trim), constants (pi/e/c).
- **Type coercion**: as_double/as_int/as_bool/as_string helpers plus cell_to_taql for CellValue↔TaqlValue conversion.
- **compare_values()**: Unified comparator for mixed-type ordering (string < number, number comparison via double promotion).
- **taql_execute()**: Full command dispatch for SELECT (WHERE, ORDERBY, DISTINCT, LIMIT/OFFSET, projection with values), UPDATE (assignments + flush), DELETE (row collection), COUNT, CALC, SHOW.
- **taql_calc()**: Standalone expression evaluation without table context.

## Design decisions

1. Integer arithmetic preserves int64_t; mixed int/float promotes to double
2. UPDATE automatically flushes table after writes (required by storage manager buffering)
3. DELETE collects matching row indices but doesn't physically remove rows (Table API has no removeRow)
4. String concatenation via `+` operator when either operand is a string
5. Short-circuit evaluation for AND/OR logical operators

## Test coverage

- Expression evaluator: int/float arithmetic, unary negation, 7 comparison operators, AND/OR/NOT, 16 math functions, 3 constants, IIF, string functions, complex expressions
- Command execution: SELECT with all clauses (WHERE, ORDERBY DESC, LIMIT/OFFSET, single-column projection), COUNT with WHERE, CALC standalone, UPDATE with write verification, DELETE row collection, SHOW text output, expressions referencing table columns

## Build & CI

- 228 tests pass across 4 TaQL test suites (51 + 24 + 145 + 8)
- Zero compiler warnings
- clang-tidy clean
