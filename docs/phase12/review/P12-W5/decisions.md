# P12-W5 Decisions

1. Multi-table execution uses a cross-product strategy: build all row combinations,
   then filter by JOIN ON conditions and WHERE clause. This is simple and correct
   for the table sizes typical in radio astronomy workflows.
2. A new `taql_execute` overload accepts `unordered_map<string, Table*>` rather than
   modifying the single-table overload, preserving backward compatibility.
3. Column qualification uses `alias.column` dotted notation resolved by `resolve_column()`.
   If no dot is found, the primary (first) table is used.
4. FROM (subquery), EXISTS, and WITH CTE execution are deferred as open issues since
   the parser support is incomplete and these features are rarely used in casacore
   radio astronomy workflows.
