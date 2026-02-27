# P11-W6 Design Decisions

## D1: String-based WHERE vs expression tree
**Choice**: Generate a TaQL WHERE clause string rather than an expression tree.
**Rationale**: casacore's `toTableExprNode()` returns a `TableExprNode` (expression tree). Since casacore-mini uses string-based TaQL execution via `taql_execute()`, a WHERE clause string is the natural equivalent. The string can be passed directly to `taql_execute("SELECT FROM t WHERE " + expr, table)`.

## D2: Helper function decomposition
**Choice**: Three internal helpers (`int_expr_to_taql`, `bound_to_taql`, `double_range_to_taql`) factored out from `to_taql_where()`.
**Rationale**: Multiple categories share the same expression patterns (comma-separated IDs, range `~`, bounds `< >`). Factoring avoids code duplication while keeping each helper focused.

## D3: Parity testing strategy
**Choice**: Every WHERE test runs both the TaQL path (`taql_execute` with WHERE) and the direct `evaluate()` path, then compares row counts.
**Rationale**: This verifies that `to_taql_where()` produces semantically equivalent results to the direct row selection engine, catching any divergence.

## D4: Accessor return type
**Choice**: `const std::optional<std::string>&` for all 12 expression accessors.
**Rationale**: Optional naturally represents "not set" vs "set with value". Returning const reference avoids copies and is consistent with the member storage type.

## D5: AND-only composition
**Choice**: Multiple category WHERE clauses are joined with `AND`.
**Rationale**: MSSelection semantics are intersection-based — all active categories must match. This mirrors the `evaluate()` logic where `selected[r]` is set to false by each category independently.
