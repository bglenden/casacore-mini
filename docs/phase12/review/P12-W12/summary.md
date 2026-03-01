# P12-W12: Storage manager fidelity re-audit + deferred-on-demand freeze

## Scope
Re-audit all six core storage managers after P12-W2 mutation additions.
Document deferred-on-demand policy for non-core managers.

## Findings

### Bug fix: TSM add_row row-count ordering
Discovered that `Table::add_row()` wrote the updated row count to `table.dat`
before re-opening TSM readers for data copy. This caused `TSM cube coverage
mismatch` errors when the reader validated the old hypercube (covering N rows)
against the updated table row count (N+1). Fixed by deferring the row-count
update to after the writer rebuild.

### TSM mutation test coverage
Added `test_add_row_tsm()` and `test_remove_row_tsm()` to
`table_mutation_test.cpp`. These exercise TiledColumnStMan add_row and
remove_row with verified on-disk roundtrip (create → write → flush →
mutate → flush → re-open → verify).

### Fidelity status
All six managers are now classified as `Exact` across all five operation
categories: create, open/read, write, add_row, remove_row.

### Deferred-on-demand policy
Published `docs/phase12/deferred_on_demand_policy.md` covering:
- MemoryStMan, Adios2StMan, Dysco, AlternateMans
- All classified as `DeferredOnDemand`
- Demand-trigger procedure: evidence → impact → scope → plan update → approval

## Test results
All 107 tests pass including new TSM mutation tests.

## Wave gate
10/10 checks pass.
