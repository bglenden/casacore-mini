# P11-W7 Design Decisions

## D1: Simplified vs Excluded status
**Choice**: Use `Simplified` for features where the parser is complete but the evaluator lacks required infrastructure (GROUPBY engine, multi-table JOIN, DDL filesystem ops, array column access, datetime framework). Use `Excluded` only for features requiring entirely new architecture (UDF plugin system, MeasUDF).
**Rationale**: Simplified rows have parser coverage and could be extended with incremental work. Excluded rows would require fundamental new subsystems.

## D2: Regex evaluation via std::regex
**Choice**: Use C++ `<regex>` with ECMAScript syntax for both `regex_expr` (~) and improved `like_expr` pattern matching.
**Rationale**: Standard library regex is portable and sufficient for TaQL patterns. Upstream casacore uses POSIX regex, but ECMAScript is a superset for common patterns.

## D3: LIKE pattern conversion to regex
**Choice**: Convert SQL/glob patterns (`%`, `_`, `*`, `?`) to regex internally, then use `std::regex_match` for full pattern support.
**Rationale**: This replaces the previous simple prefix/suffix-only matching with complete pattern support. The regex conversion handles special character escaping.

## D4: ROWNR()/ROWID() context propagation
**Choice**: Pass `EvalContext` to `eval_math_func` so ROWNR()/ROWID() can return the actual row number.
**Rationale**: Previous implementation returned a stub value of 0. The EvalContext already carries the current row index for WHERE evaluation.

## D5: Checklist traceability
**Choice**: Updated all checklist rows with `status`, `commit_sha`, `evidence_path`, and `notes` fields.
**Rationale**: W7 plan requires zero open Pending rows and full traceability for Done rows.
