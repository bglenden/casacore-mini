# P11-W5 Design Decisions

## D1: Feed pair syntax parity
Adopted casacore's feed/antenna pair syntax: `f1&f2` (cross-only), `f1&&f2` (with auto), `f1&&&` (auto-only). Negation prefix `!` inverts the match.

## D2: TaQL injection via taql_execute
Raw TaQL expressions are injected by constructing `SELECT FROM t WHERE <expr>` and executing via `taql_execute()`. The resulting row set is ANDed with other category selections. This wires W4's evaluator into the MS selection engine.

## D3: Scan/observation/array bound operators
Added `<N` and `>N` bound syntax for scan, observation, and array categories (matching upstream casacore grammar). These are strict inequalities (not inclusive).

## D4: Clear/reset semantics
`clear()` resets all 12 optional expression fields to `nullopt`. `reset()` is an alias for `clear()` for API compatibility with upstream MSSelection.

## D5: Result struct expansion
Added `observations`, `arrays`, `feeds` vectors to `MsSelectionResult` alongside existing category results. Each populated by its respective parser during evaluation.
