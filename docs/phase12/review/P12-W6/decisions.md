# P12-W6 Decisions

1. Aggregate functions use the `g` prefix naming convention (GCOUNT, GSUM, etc.)
   matching casacore TaQL syntax rather than SQL standard (COUNT, SUM, etc.).
2. When a SELECT contains aggregates but no GROUPBY clause, all matching rows
   are treated as a single group — consistent with SQL semantics.
3. HAVING expressions are evaluated using `eval_with_aggregates()` which
   recursively resolves aggregate sub-expressions before evaluating the
   overall condition.
4. Population variance/stddev formulas are used (divide by N), matching
   casacore conventions.
