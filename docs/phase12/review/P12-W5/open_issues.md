# P12-W5 Open Issues

- FROM (subquery) parsing is stubbed: the parser skips content and assigns "(subquery)"
  placeholder. Full subquery parsing requires recursive parser invocation.
- EXISTS subquery evaluation is not implemented. The parser constructs
  `ExprType::exists_expr` but eval_expr has no handler.
- WITH CTE resolution is parsed but not executed. CTE tables are stored in
  `ast.with_tables` but not resolved during evaluation.
- Recursive CTEs are out of scope for Phase 12.
- Subqueries in WHERE/HAVING clauses (scalar subqueries) are not yet supported.
