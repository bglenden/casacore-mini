# P11-W3 Review Summary: TaQL parser + AST foundation

## Deliverables

| Artifact | Status |
|---|---|
| `include/casacore_mini/taql.hpp` | Complete — full AST types, 10 command enums, 25+ expression types, public API |
| `src/taql.cpp` | Complete — hand-written lexer, recursive-descent parser, SHOW help text, evaluator stubs |
| `tests/taql_parser_test.cpp` | 137 checks across all 10 commands, expression types, operators, literals |
| `tests/taql_malformed_test.cpp` | 8 checks for error paths |
| `tools/check_phase11_w3.sh` | 63/63 gate checks pass |

## Architecture

- **Lexer**: `TaqlLexer` hand-written tokenizer supporting strings, regex, numbers (decimal/hex/complex), identifiers, all operators and punctuation.
- **Parser**: `TaqlParser` recursive-descent with precedence climbing for expressions. Parses all 10 TaQL command families: SELECT, UPDATE, INSERT, DELETE, COUNT, CALC, CREATE TABLE, ALTER TABLE, DROP TABLE, SHOW.
- **AST**: `TaqlAst` top-level struct plus `TaqlExprNode` expression tree. 25+ expression node types covering the full TaQL expression grammar.
- **Evaluator stubs**: `taql_execute()` and `taql_calc()` exist but return empty results (populated in W4).

## Design decisions

1. Hand-written recursive-descent parser (not bison/flex) for cleaner C++23 integration
2. Single header + single source for the full TaQL module
3. Precedence climbing for expression parsing: OR → AND → NOT → comparison → bitwise → additive → multiplicative → power → unary → postfix → primary
4. `peek_next_is()` uses lexer snapshot/restore for `&&`/`||` disambiguation

## Test coverage

- All 10 command types parse correctly
- Expression precedence verified (arithmetic, power right-associativity)
- All comparison operators (=, ==, !=, <>, <, >, <=, >=, ~=, !~=)
- Logical operators (AND, OR, NOT)
- Set operations (IN, BETWEEN, LIKE, ILIKE)
- Function calls and aggregate functions
- Literals: bool, int, float, complex, hex, string
- Keywords (col::key), dotted column refs
- Error paths: unclosed parens, missing keywords, invalid tokens

## Build & CI

- 86/86 tests pass (84 existing + 2 new)
- Zero compiler warnings
- clang-tidy clean (only toolchain `<complex>` lookup issue remains)
