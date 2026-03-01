# Phase 12 Plan (Functional Closure + mdspan Modernization)

Date: 2026-03-01
Status: Done

## Objective

Phase 12 is the functional closure phase.

It must deliver all remaining compatibility-critical capabilities still marked
`Simplified` or equivalent partial coverage after Phase 11, while preserving
on-disk interoperability and then completing internal mdspan modernization.

Owner decisions captured for this phase:

1. The currently implemented storage-manager set must be fully faithful:
   `StandardStMan`, `IncrementalStMan`, `TiledColumnStMan`,
   `TiledCellStMan`, `TiledShapeStMan`, `TiledDataStMan`.
2. Additional storage managers not yet in `casacore-mini`
   (`MemoryStMan`, `Adios2StMan`, `AlternateMans`, `Dysco`) are explicitly
   demand-driven and do not block Phase-12 closeout.
3. TaQL measure/UDF functionality does not need plugin architecture parity;
   built-in equivalents are acceptable if behavior is equivalent.
4. Table infrastructure parity surfaces are in scope (locking, indexing,
   reference/concat views, iterators, proxies/utility surfaces).

## Entry condition

Phase 12 starts when all are true:

1. `bash tools/check_ci_build_lint_test_coverage.sh` passes on the branch
   baseline in a non-casacore host environment.
2. A casacore-enabled host is available for matrix runs.
3. `docs/casacore_plan.md` and this file agree on Phase-12 scope.

## Definition of done

Phase 12 is complete only when all are true:

1. Phase-8 carry-forward coordinate regression is closed:
   - `coord-keywords mini->casacore` pass
   - `mixed-coords mini->casacore` pass
2. TaQL closure:
   - all required command families execute, not parser-only
   - `INSERT` and `DELETE` operate through table mutation APIs
   - `CREATE/ALTER/DROP TABLE` execute with filesystem effects
   - `JOIN`, `GROUPBY`, `HAVING`, aggregate paths execute correctly
   - no required row in `docs/phase11/taql_command_checklist.csv` remains
     `Simplified` unless explicitly reclassified as demand-driven with owner
     approval
3. MSSelection closure:
   - required parser and evaluator parity for all 12 categories
   - accessors required by upstream workflows are implemented or mapped to
     equivalent behavior with explicit tests
   - `PARSE_NOW`/`PARSE_LATE` behavior parity exists
   - non-throwing error collection mode equivalent to upstream error-handler
     behavior exists
4. Table infrastructure closure:
   - locking, indexing, ref/concat view behavior, and iterator/query support are
     implemented with executable tests
5. Storage manager policy is enforced:
   - current six-manager set remains fidelity-exact with read/write/create/mutate
     validation
   - non-core managers are documented as `DeferredOnDemand` with trigger policy
6. mdspan modernization complete for planned internal array paths with no API or
   on-disk format break.
7. Full quality gates pass with no required skip.
8. Phase docs, review packets, and rolling-plan status are consistent with code
   and checks.

## Scope boundaries

### In scope

1. Coordinate interop carry-forward regression closure.
2. TaQL execution parity closure for command, expression, and evaluator gaps.
3. MSSelection parity closure for parser/evaluator/accessor/error-mode gaps.
4. Table infrastructure parity tranche:
   - `TableLock` semantics
   - `ColumnsIndex`/index-query support
   - `RefTable` and `ConcatTable`
   - iterator surfaces
   - table metadata/proxy/utility surfaces needed by TaQL/MSSelection and
     compatibility workflows
5. Built-in MeasUDF-equivalent TaQL functionality (no plugin requirement).
6. mdspan internal migration.
7. Phase-12 wave checks, matrix checks, and review evidence scaffolding.

### Out of scope

1. New scientific algorithm families not required for compatibility closure
   (for example `scimath` equivalents).
2. Full plugin ABI/UDF plugin loading infrastructure.
3. Non-core storage managers unless explicitly pulled in by new demand.
4. Performance-only tuning not required for correctness parity.

## Demand-driven deferred set (non-blocking)

These are tracked but do not block Phase-12 closeout.

| Area | Upstream examples | Status | Trigger to promote into scope |
|---|---|---|---|
| Extra storage managers | `MemoryStMan`, `Adios2StMan`, `Dysco`, `AlternateMans` | DeferredOnDemand | A concrete user workflow or interop corpus artifact requires one |
| FITS wrapper stack | `fits/*`, `msfits/*` wrapper classes | DeferredOnDemand | Direct cfitsio + existing tools proven insufficient |
| Plugin UDF framework | `tables/TaQL/UDFBase` dynamic plugin patterns | DeferredOnDemand | Built-in UDF equivalents cannot cover required TaQL behavior |

## Execution order and dependencies (strict)

Waves are sequential and fail-fast:

1. `P12-W1` Coordinate regression closure + matrix diagnostics hardening
2. `P12-W2` Table mutation foundation (`add_row`, `remove_row`) and tests
3. `P12-W3` TaQL DML closure (`INSERT`, `DELETE`) using mutation APIs
4. `P12-W4` TaQL DDL closure (`CREATE`, `ALTER`, `DROP`, `INTO`, `GIVING`, `DMINFO`)
5. `P12-W5` TaQL multi-table closure (`JOIN`, multi-FROM, subqueries, CTEs)
6. `P12-W6` TaQL grouping/aggregate closure (`GROUPBY`, `HAVING`, aggregate funcs)
7. `P12-W7` TaQL expression-function closure (array, datetime, angle, unit,
   cone, complex math, built-in MeasUDF equivalents)
8. `P12-W8` MSSelection parser/evaluator closure (regex, names, freq/time/uv units)
9. `P12-W9` MSSelection API/accessor/mode/error closure (`PARSE_LATE`,
   error-handler equivalent, full accessor semantics, `toTableExprNode` parity)
10. `P12-W10` Table infrastructure tranche A (`TableLock`, metadata/util,
    setup helpers, lock-aware mutation)
11. `P12-W11` Table infrastructure tranche B (`RefTable`, `ConcatTable`,
    iterators, index surfaces, proxy compatibility touchpoints)
12. `P12-W12` Storage-manager fidelity re-audit + demand-driven policy freeze
13. `P12-W13` mdspan foundation + migration tranches
14. `P12-W14` Full closeout (fresh-build aggregate checks + docs)

No user confirmation pauses between waves during normal execution.

## Required quality and documentation gates (every wave)

Mandatory commands:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. `bash tools/check_phase12_wX.sh` for current wave

Phase aggregate commands:

1. `bash tools/check_phase12.sh`
2. `bash tools/interop/run_phase8_matrix.sh <build_dir>` (casacore-enabled host)
3. `bash tools/interop/run_phase10_matrix.sh <build_dir>` (casacore-enabled host)
4. `bash tools/interop/run_phase11_matrix.sh <build_dir>` (if present for TaQL/MSSel)

Gate rules:

1. Warnings-as-errors and clang-tidy remain mandatory.
2. Required checks may not be downgraded to informational.
3. Functional claims require executable checks; no grep-only proof.
4. Verifier/runner crashes are automatic wave failure.
5. Command exit status is the pass/fail truth source.
6. Any compatibility-facing API map row claimed `Done` must cite symbol and
   exercising test evidence.

## Workstream table

| Wave | Status | Deliverables |
|---|---|---|
| `P12-W1` | Done | coordinate regression fixed; matrix diagnostics hardened |
| `P12-W2` | Done | table row-mutation APIs + tests |
| `P12-W3` | Done | TaQL INSERT/DELETE execution parity |
| `P12-W4` | Done | TaQL DDL execution parity |
| `P12-W5` | Done | TaQL multi-table/JOIN/subquery/CTE execution |
| `P12-W6` | Done | TaQL GROUPBY/HAVING/aggregate execution |
| `P12-W7` | Done | TaQL function-expression closure + built-in MeasUDF equivalents |
| `P12-W8` | Done | MSSelection parser/evaluator parity closure |
| `P12-W9` | Done | MSSelection API/accessor/mode/error parity closure |
| `P12-W10` | Done | table infrastructure tranche A |
| `P12-W11` | Done | table infrastructure tranche B |
| `P12-W12` | Done | storage-manager re-audit + deferred-on-demand freeze |
| `P12-W13` | Done | mdspan foundation and migrations |
| `P12-W14` | Done | final closeout and evidence pack |

## Per-wave implementation details

### `P12-W1` coordinate regression closure + diagnostics

Implementation tasks:

1. Reproduce failing coordinate cells with visible stderr.
2. Update semantic verifier field-location matching for both spectral layouts.
3. Remove silent stderr suppression in matrix cell runner.
4. Emit per-cell diagnostics (artifact, direction, command pair, stderr, path).
5. Prove 24/24 Phase-8 matrix pass on casacore host.

Expected touchpoints:

1. `tools/interop/casacore_interop_tool.cpp`
2. `tools/interop/run_phase8_matrix.sh`
3. `docs/phase8/known_differences.md` (if needed)

Wave gate:

1. `bash tools/check_phase12_w1.sh <build_dir>`

### `P12-W2` table mutation foundation

Implementation tasks:

1. Add `Table::add_row(std::uint64_t n = 1)` and `Table::remove_row(std::uint64_t row)`
   (or equivalent batch form).
2. Implement mutation routing in all currently supported storage managers.
3. Preserve table-level and column-level metadata consistency after mutation.
4. Add mutation tests for scalar and array columns for each manager family.

Expected touchpoints:

1. `include/casacore_mini/table.hpp`
2. `src/table.cpp`
3. `src/standard_stman.cpp`
4. `src/incremental_stman.cpp`
5. `src/tiled_stman.cpp`
6. new tests under `tests/*table*` and `tests/*stman*`

Wave gate:

1. `bash tools/check_phase12_w2.sh <build_dir>`

### `P12-W3` TaQL DML closure (INSERT/DELETE)

Implementation tasks:

1. Implement `INSERT ... VALUES`, `INSERT ... SET`, `INSERT ... SELECT` execution.
2. Implement `DELETE FROM ... WHERE ... [ORDERBY/LIMIT]` execution.
3. Ensure affected-row counts and flush semantics match expected behavior.
4. Add negative tests for malformed and unsupported combinations.

Expected touchpoints:

1. `src/taql.cpp`
2. `include/casacore_mini/taql.hpp`
3. `tests/taql_command_test.cpp`
4. new focused `tests/taql_dml_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w3.sh <build_dir>`

### `P12-W4` TaQL DDL closure (CREATE/ALTER/DROP)

Implementation tasks:

1. Implement `CREATE TABLE` execution with column specs and initial rows.
2. Implement `ALTER TABLE` actions (add/copy/rename/drop column; keyword ops;
   add row).
3. Implement `DROP TABLE` execution with safety checks.
4. Implement `SELECT INTO`, `GIVING`, and `DMINFO` paths where required for
   parity workflows.

Expected touchpoints:

1. `src/taql.cpp`
2. `src/table.cpp`
3. `include/casacore_mini/table.hpp`
4. `tests/taql_command_test.cpp`
5. new `tests/taql_ddl_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w4.sh <build_dir>`

### `P12-W5` TaQL multi-table/JOIN/subquery/CTE closure

Implementation tasks:

1. Add table registry/context for multi-table resolution.
2. Execute multi-table `FROM` and `JOIN ... ON` (including multiple joins).
3. Execute `EXISTS` and `FROM (subquery)` paths.
4. Execute `WITH` CTE resolution for required non-recursive cases first,
   then recursive/stack-safe behavior as needed.

Expected touchpoints:

1. `src/taql.cpp`
2. new helper sources under `src/taql_*`
3. tests: `tests/taql_join_test.cpp`, `tests/taql_subquery_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w5.sh <build_dir>`

### `P12-W6` TaQL grouping/aggregate closure

Implementation tasks:

1. Implement grouping engine for `GROUPBY` and `ROLLUP`.
2. Implement `HAVING` filtering over grouped rows.
3. Implement required aggregate function families and array-aggregate variants.
4. Verify stable behavior for grouped projection, ordering, and limits.

Expected touchpoints:

1. `src/taql.cpp`
2. new group/aggregate helpers under `src/taql_*`
3. tests: `tests/taql_aggregate_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w6.sh <build_dir>`

### `P12-W7` TaQL expression-function closure + built-in MeasUDF equivalents

Implementation tasks:

1. Close array expression evaluation (`subscript`, array funcs, masked arrays).
2. Close unit-suffix conversion semantics.
3. Close datetime and angle function semantics.
4. Close cone-search and complex arithmetic semantics.
5. Implement built-in measure conversion functions equivalent to required
   MeasUDF families (no plugin loader requirement).

Expected touchpoints:

1. `src/taql.cpp`
2. `src/measure_convert.cpp` (if needed by TaQL function bridge)
3. `include/casacore_mini/taql.hpp`
4. tests: `tests/taql_eval_test.cpp`, new `tests/taql_udf_equiv_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w7.sh <build_dir>`

### `P12-W8` MSSelection parser/evaluator closure

Implementation tasks:

1. Add missing regex/pattern semantics for antenna/field/SPW as required.
2. Add SPW selection by name/frequency/step and channel-list variants.
3. Add time-string parser support and wildcard fields.
4. Add UV unit handling (`m`, `km`, `wavelength`) and percentage syntax.
5. Add missing state bounds and baseline-length/station-location semantics.

Expected touchpoints:

1. `src/ms_selection.cpp`
2. `include/casacore_mini/ms_selection.hpp`
3. tests: `tests/ms_selection_test.cpp`,
   `tests/ms_selection_parser_test.cpp`,
   new `tests/ms_selection_parity_extended_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w8.sh <build_dir>`

### `P12-W9` MSSelection API/accessor/mode/error closure

Implementation tasks:

1. Implement full accessor parity where required:
   - baseline matrix and split antenna/feed lists
   - time/uv range outputs and unit flags
   - DDID/polarization map outputs
2. Implement `PARSE_LATE` semantics in addition to parse-now.
3. Implement error-collection handler mode equivalent to upstream behavior.
4. Ensure `toTableExprNode` parity path is executable in active query paths
   (string bridge acceptable only if behavior is proven equivalent).

Expected touchpoints:

1. `include/casacore_mini/ms_selection.hpp`
2. `src/ms_selection.cpp`
3. tests: `tests/ms_selection_table_expr_bridge_test.cpp`,
   new `tests/ms_selection_api_parity_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w9.sh <build_dir>`

### `P12-W10` table infrastructure tranche A

Implementation tasks:

1. Implement lock model and lock-aware mutation/read APIs.
2. Implement core table metadata/info/utility surfaces needed by workflows.
3. Add setup/new-table helper surfaces needed by TaQL DDL paths.
4. Add tests for lock conflicts, metadata persistence, and utility behavior.

Expected touchpoints:

1. new headers/sources under `include/casacore_mini/` and `src/`
   for lock/info/util/setup components
2. `src/table.cpp`
3. tests: new `tests/table_lock_test.cpp`, `tests/table_info_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w10.sh <build_dir>`

### `P12-W11` table infrastructure tranche B

Implementation tasks:

1. Implement reference-table/view behavior (`RefTable` equivalent).
2. Implement concatenated-table behavior (`ConcatTable` equivalent).
3. Implement iterator/index surfaces needed by query and selection workflows.
4. Implement required proxy-facing compatibility touchpoints.

Expected touchpoints:

1. new headers/sources under `include/casacore_mini/` and `src/`
2. tests: new `tests/ref_table_test.cpp`, `tests/concat_table_test.cpp`,
   `tests/columns_index_test.cpp`, `tests/table_iter_test.cpp`

Wave gate:

1. `bash tools/check_phase12_w11.sh <build_dir>`

### `P12-W12` storage-manager fidelity re-audit + deferred-on-demand freeze

Implementation tasks:

1. Re-audit current six-manager behavior after mutation and DDL additions.
2. Update storage-manager fidelity doc with exactness evidence.
3. Publish explicit `DeferredOnDemand` policy rows for non-core managers.
4. Add demand-trigger procedure (what evidence promotes a deferred manager).

Expected touchpoints:

1. `docs/phase11/storage_manager_fidelity_audit.md`
2. `docs/phase12/*` policy and review docs
3. matrix scripts/tests as required

Wave gate:

1. `bash tools/check_phase12_w12.sh <build_dir>`

### `P12-W13` mdspan foundation + migration

Implementation tasks:

1. Introduce mdspan facade and owner/view adapters.
2. Migrate selected lattice/image/table internal multidim paths.
3. Preserve external API and file-format behavior.
4. Add parity tests proving pre/post equivalence.

Expected touchpoints:

1. `include/casacore_mini/mdspan_compat.hpp`
2. new array helper headers/sources
3. selected `src/lattice*`, `src/image*`, `src/table*`
4. tests for migrated paths

Wave gate:

1. `bash tools/check_phase12_w13.sh <build_dir>`

### `P12-W14` final closeout

Implementation tasks:

1. Run all required gates from a fresh build directory.
2. Run required interop matrix suites on casacore-enabled host.
3. Reconcile checklist statuses and known differences.
4. Publish `docs/phase12/exit_report.md` and complete review packets.

Expected touchpoints:

1. `docs/phase12/exit_report.md`
2. `docs/phase12/review/P12-WX/*`
3. `docs/casacore_plan.md`
4. closure updates in phase11 checklist docs if reused as source of truth

Wave gate:

1. `bash tools/check_phase12_w14.sh <build_dir>`

## Interoperability and evaluation contract

Required evidence:

1. Phase-8 matrix: 24/24 pass.
2. Phase-10 matrix: all required lattice/image cells pass.
3. TaQL parity suite: command + expression + malformed cases.
4. MSSelection parity suite: category parser + evaluator + bridge + accessors.
5. Table infrastructure suite: lock/index/ref/concat/iter behavior tests.
6. Storage manager suite: create/open/read/write/mutate across all six managers.

## Required review artifacts (per wave)

Each completed wave must include:

1. `docs/phase12/review/P12-WX/summary.md`
2. `docs/phase12/review/P12-WX/files_changed.txt`
3. `docs/phase12/review/P12-WX/check_results.txt`
4. `docs/phase12/review/P12-WX/open_issues.md`
5. `docs/phase12/review/P12-WX/decisions.md`

Phase status file:

1. `docs/phase12/review/phase12_status.json`

## Immediate next step

Start `P12-W1`.

1. Reproduce both coordinate failures with visible stderr.
2. Patch semantic verifier field-location handling.
3. Re-run full Phase-8 matrix on casacore host.
4. Commit matrix diagnostics hardening.

## Autonomy policy

During Phase 12 execution, the implementing agent proceeds wave-by-wave without
waiting for user confirmation except when:

1. privileged environment changes or credentials are required,
2. a plan-level scope change is needed,
3. destructive repository actions are requested.

## Wave-gate design rules

1. No grep-only gates for functional claims.
2. Every functional gate executes real code paths.
3. Producer and verifier command failures are hard failures.
4. Segfaults or verifier crashes fail the wave immediately.

## Closeout evidence protocol

1. Final closeout must use a fresh build directory.
2. Review-packet command outputs must come from current branch state.
3. Plan, exit report, and rolling-plan status text must match executable
   evidence.

## Lint profile policy

1. Lock active lint profile at Phase-12 kickoff.
2. Any lint-profile change requires:
   - explicit plan update
   - rationale
   - note in the corresponding review-packet decisions file

## API conformance policy

For compatibility-facing rows in API maps/checklists:

1. each row claimed `Done` must cite exact symbol(s),
2. each row must cite at least one exercising test,
3. rows with behavior-level equivalence (different API shape) must document the
   equivalence mapping and proof test.
