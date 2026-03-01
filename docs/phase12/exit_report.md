# Phase 12 Exit Report

## Summary

Phase 12 (Functional Closure + mdspan Modernization) completed all 14 waves,
delivering compatibility-critical table infrastructure, TaQL/MSSelection
execution parity, storage-manager fidelity, and mdspan internal migration.

## Wave Results

| Wave | Description | Tests Added | Status |
|------|-------------|-------------|--------|
| W1 | Coordinate regression closure + diagnostics | matrix hardening | Done |
| W2 | Table mutation foundation (add_row/remove_row) | table_mutation_test | Done |
| W3 | TaQL DML closure (INSERT/DELETE) | taql_dml_test | Done |
| W4 | TaQL DDL closure (CREATE/ALTER/DROP) | taql_ddl_test | Done |
| W5 | TaQL multi-table/JOIN/subquery/CTE | taql_join_test | Done |
| W6 | TaQL grouping/aggregate closure | taql_aggregate_test | Done |
| W7 | TaQL expression-function closure + MeasUDF | taql_udf_equiv_test | Done |
| W8 | MSSelection parser/evaluator closure | ms_selection_parity_extended_test | Done |
| W9 | MSSelection API/accessor/mode/error closure | ms_selection_api_parity_test | Done |
| W10 | Table infrastructure tranche A | table_lock_test, table_info_test | Done |
| W11 | Table infrastructure tranche B | ref_table_test, concat_table_test, table_iter_test, columns_index_test | Done |
| W12 | Storage-manager fidelity re-audit | TSM mutation tests added | Done |
| W13 | mdspan foundation + migration | mdspan_migration_test (50 checks) | Done |
| W14 | Final closeout | aggregate verification | Done |

## Definition of Done Checklist

1. **Coordinate regression closure**: W1 remediated Phase-8 carry-forward
   coordinate cells. Matrix diagnostics hardened with visible stderr and
   per-cell output.

2. **TaQL closure**: All required command families execute (not parser-only).
   INSERT/DELETE (W3), CREATE/ALTER/DROP TABLE (W4), JOIN/multi-FROM/CTE (W5),
   GROUPBY/HAVING/aggregates (W6), and expression-function families (W7) all
   have executable tests.

3. **MSSelection closure**: Parser and evaluator parity for all 12 categories
   (W8). PARSE_NOW/PARSE_LATE behavior parity, error collection mode, and full
   accessor semantics (W9). toTableExprNode parity path tested via bridge
   tests.

4. **Table infrastructure closure**: TableLock semantics, TableInfo metadata
   (W10). RefTable, ConcatTable, TableIterator, ColumnsIndex (W11). All
   exercised by executable tests.

5. **Storage manager policy enforced**: Six-manager set remains fidelity-exact
   across create/open/read/write/add_row/remove_row (W12). Non-core managers
   documented as DeferredOnDemand with trigger policy
   (docs/phase12/deferred_on_demand_policy.md).

6. **mdspan modernization**: strided_fortran_copy/scatter helpers and
   make_*_lattice_span convenience functions added. LatticeArray get_slice/
   put_slice refactored to use mdspan-style stride arithmetic. No API or
   on-disk format changes (W13).

7. **Quality gates**: 108/108 tests pass from fresh build. All 13 wave gates
   pass. Warnings-as-errors enforced throughout.

8. **Documentation**: Review packets complete for all 14 waves. Plan, exit
   report, and rolling-plan status consistent with code and checks.

## Key Bug Fixes

1. **TSM add_row row-count ordering (W12)**: Table::add_row() wrote updated
   row_count to table.dat before re-opening TSM readers, causing "cube
   coverage mismatch" errors. Fixed by deferring row_count update to after
   writer rebuild.

2. **W4 gate pattern (W14)**: drop-table-executes check grepped for
   `remove_all` instead of `drop_table`. Corrected during closeout.

## Test Suite

- Total tests: 108
- All pass from fresh build directory (build-closeout)
- Wall time: ~24 seconds

## Open Items

1. **Interop matrix runs**: Phase-8/10/11 matrix suites require casacore-enabled
   host. Local wave gates validate functional claims; full interop deferred.

2. **vector<bool> mdspan limitation**: strided_fortran_copy/scatter cannot be
   used with vector<bool> due to proxy reference semantics. bool specialization
   retains element-wise fallback.

3. **Additional mdspan migration candidates**: LatticeIterator position tracking
   and RebinLattice bin iteration could benefit from strided helpers but work
   correctly as-is.
