# Phase 11 Detailed Plan: Full TaQL + MSSelection Support

Date: 2026-02-27
Status: Pending (authoritative implementation playbook for `P11-W3`..`P11-W7`)

## 1. Non-negotiable outcome

This plan closes two required capabilities with full upstream parity:

1. `tables/TaQL` full command and expression support
2. `ms/MSSel` full `MSSelection` support, including `toTableExprNode` behavior

No simplified subset is acceptable for phase closeout.

## 2. Upstream source-of-truth files

Implementation and parity checks must be anchored to these upstream files in
`casacore-original/`:

1. TaQL grammar/parser:
   - `tables/TaQL/TableGram.yy`
   - `tables/TaQL/TableGram.ll`
   - `tables/TaQL/TableParse*.cc`
   - `tables/TaQL/TaQLNode*.cc`
   - `tables/TaQL/Expr*Node*.cc`
2. TaQL command/eval support:
   - `tables/TaQL/TaQLResult.*`
   - `tables/TaQL/TaQLJoin.*`
   - `tables/TaQL/TaQLShow.*`
3. MS selection:
   - `ms/MSSel/MSSelection.*`
   - `ms/MSSel/MSSelectionKeywords.*`
   - category grammars/parsers/index helpers under `ms/MSSel/MS*Gram.*`,
     `MS*Parse.*`, `MS*Index.*`

## 3. Mandatory feature closure checklist

`P11-W1` must populate and maintain:

1. `docs/phase11/taql_command_checklist.csv`
2. `docs/phase11/msselection_capability_checklist.csv`

Required columns:

1. `family`
2. `feature`
3. `upstream_source`
4. `mini_symbol`
5. `status` (`Pending`, `InProgress`, `Done`, `Blocked`)
6. `target_wave`
7. `test_id`
8. `interop_artifact`
9. `commit_sha`
10. `pr_link`
11. `evidence_path`
12. `oracle_diff_path`
13. `notes`

Rules:

1. `oracle_diff_path` is `none` only for rows where no oracle comparison is
   meaningful (must be justified in `notes`).
2. `evidence_path` points to exact log/report files committed in the repository.
3. `commit_sha` and `pr_link` are mandatory once `status=Done`.

Rows are complete only when `mini_symbol + test_id + interop_artifact +
commit_sha + pr_link + evidence_path + oracle_diff_path` are all non-empty and
verified under the rules above.

## 4. TaQL full-support contract

Required command families (from `TableGram`):

1. `SELECT` (including `FROM`, `JOIN`, `WHERE`, `GROUPBY`, `HAVING`,
   `ORDERBY`, `LIMIT/OFFSET`, `GIVING`/`INTO`, `DISTINCT`/`ALL`)
2. `UPDATE`
3. `INSERT` (values and `INSERT ... SELECT`)
4. `DELETE`
5. `COUNT`
6. `CALC`
7. `CREATETAB`
8. `ALTERTAB`
9. `DROPTAB`
10. `SHOW`

Required expression support:

1. arithmetic/logical/comparison precedence parity
2. `IN`, `BETWEEN`, regex operators, wildcard projection
3. set/range/subscript/record literal forms used in upstream grammar
4. function calls, aggregate functions, and unit-aware forms used by TaQL
5. `WITH` and nested/subquery command handling

## 5. MSSelection full-support contract

Required category/parsing families:

1. antenna
2. field
3. spw/channel
4. scan
5. array
6. time
7. uvdist
8. correlation/poln
9. state
10. observation
11. feed
12. raw TaQL expression injection

Required behavior:

1. parse-now and parse-late semantics
2. expression setter/getter surface parity
3. category composition parity (AND and category-specific rules)
4. selected-ID accessors parity for category outputs
5. `to_table_expr_node` parity and active-path integration

## 6. Implementation constraints

1. Do not bypass `Table` public data-access APIs for cell reads/writes.
2. Do not add "temporary simplified parser" code paths.
3. Do not mark rows `Done` based only on parser acceptance; evaluator parity is
   required.
4. Keep all new public APIs documented in Doxygen in the same wave as code.
5. Use `casacore-original` as oracle for behavior, not only local assumptions.

## 7. Wave-by-wave execution (`P11-W3`..`P11-W7`)

## `P11-W3` TaQL parser + AST foundation

Execution steps:

1. Add parser/AST modules and CMake wiring.
2. Implement tokenizer and grammar for all command families in Section 4.
3. Implement typed AST nodes for commands, expressions, sets, ranges, records.
4. Implement parser diagnostics with location and token context.
5. Add parser-only test suites:
   - `tests/taql_parser_test.cpp`
   - `tests/taql_malformed_test.cpp`
6. Create parser oracle fixtures via `casacore-original` and compare parse
   intent (command type, projection list, where/group/order/limit presence).
7. Add/enable `tools/check_phase11_w3.sh`.

Mandatory gate outputs:

1. parser tests pass
2. malformed tests pass
3. checklist rows for parser-scope features move to `Done`

## `P11-W4` TaQL evaluator + command execution

Execution steps:

1. Implement expression evaluator with typed value semantics and error behavior.
2. Implement command executors for all command families in Section 4.
3. Wire TaQL entry points into table-query API surface.
4. Add command behavior tests:
   - `tests/taql_eval_test.cpp`
   - `tests/table_query_test.cpp`
   - `tests/taql_command_test.cpp`
5. Add interop/oracle runners comparing results vs `casacore-original`.
6. Add/enable `tools/check_phase11_w4.sh`.

Mandatory gate outputs:

1. command parity tests pass vs oracle
2. no command family remains parser-only
3. checklist rows for execution-scope features move to `Done`

## `P11-W5` MSSelection parser parity

Execution steps:

1. Expand `ms_selection` API to include upstream-equivalent expression families.
2. Implement category parser modules and canonical selection AST/state model.
3. Add parser tests for each category:
   - `tests/ms_selection_parser_test.cpp`
   - `tests/ms_selection_malformed_test.cpp`
4. Add parser oracle fixtures from `casacore-original` category parsers.
5. Add/enable `tools/check_phase11_w5.sh`.

Mandatory gate outputs:

1. all category parsers pass parity fixtures
2. malformed category coverage is complete
3. checklist rows for parser-scope MSSelection features move to `Done`

## `P11-W6` MSSelection evaluator + `to_table_expr_node`

Execution steps:

1. Implement selection evaluation parity over MeasurementSet tables.
2. Implement `to_table_expr_node` equivalent behavior and expose API.
3. Ensure current `evaluate()` behavior is driven by the expression-node path
   (single source of truth).
4. Add parity tests:
   - `tests/ms_selection_parity_full_test.cpp`
   - `tests/ms_selection_table_expr_bridge_test.cpp`
5. Add oracle checks for selected rows + category IDs + expression behavior.
6. Add/enable `tools/check_phase11_w6.sh`.

Mandatory gate outputs:

1. `to_table_expr_node` parity checks pass
2. row-selection parity checks pass for all categories and mixed expressions
3. checklist rows for evaluation/bridge MSSelection features move to `Done`

## `P11-W7` TaQL + MSSelection closure

Execution steps:

1. Close all remaining open rows in both checklists.
2. Resolve any `Simplified` markers to `Exact` for TaQL/MSSelection rows.
3. Add integrated end-to-end tests:
   - TaQL query over MS tables
   - MSSelection including raw TaQL clauses
4. Add documentation and API-surface reconciliation updates.
5. Add/enable `tools/check_phase11_w7.sh`.

Mandatory gate outputs:

1. zero open `Include` rows for TaQL/MSSelection
2. integrated end-to-end suites pass
3. API map and Doxygen are synchronized with implemented surface

## 8. Matrix and oracle evidence requirements

`P11-W9` and `P11-W10` must include TaQL/MSSelection artifacts in 2x2 matrix
cells (`casa->casa`, `casa->mini`, `mini->casa`, `mini->mini`).

Minimum required artifact families:

1. `taql-select-basic`
2. `taql-select-join`
3. `taql-select-groupby-having`
4. `taql-update-insert-delete`
5. `taql-calc-count-show`
6. `mssel-antenna-field-spw`
7. `mssel-time-uvdist-state`
8. `mssel-observation-feed-poln`
9. `mssel-taql-bridge`

Each family must have:

1. producer command definitions for both toolchains
2. semantic verifier in both toolchains
3. malformed/corrupt variant tests where applicable

## 9. Review packet and status artifacts

Every wave must publish reviewer-facing artifacts:

1. `docs/phase11/review/P11-WX/review_packet.md` (use the repository template
   and instantiate `P11-WX` with the concrete wave ID).
2. `docs/phase11/review/phase11_status.json` updated by wave gate scripts.

`phase11_status.json` minimum schema:

1. `phase`
2. `wave`
3. `timestamp_utc`
4. `gate_results` (per-script pass/fail + command)
5. `checklist_totals` (rows by `status`, `target_wave`)
6. `open_blockers`
7. `new_evidence_paths`
8. `matrix_summary` (for `W9` and `W10`)

## 10. Less-capable-agent operating procedure

Follow this exact cycle for each checklist row:

1. read upstream source file and identify concrete behavior
2. implement minimal code needed for that behavior
3. add one positive and one malformed/negative test
4. add or update interop/oracle artifact if row is compatibility-facing
5. run wave gate script
6. update `docs/phase11/review/P11-WX/review_packet.md` with evidence links
7. confirm `docs/phase11/review/phase11_status.json` was updated by the gate
8. only then set row `status=Done`

If blocked:

1. set `status=Blocked`
2. record blocker in `docs/phase11/review/P11-WX/open_issues.md`
3. continue with next independent row

## 11. Completion rule for this playbook

This playbook is complete only when:

1. both checklist files have no `Pending`/`InProgress`/`Blocked` rows for
   `Include` items
2. `tools/check_phase11_w3.sh` .. `tools/check_phase11_w7.sh` all pass
3. TaQL/MSSelection matrix artifacts pass all required 2x2 cells
4. every completed wave has a populated
   `docs/phase11/review/P11-WX/review_packet.md`
5. `docs/phase11/review/phase11_status.json` reflects the latest successful run
