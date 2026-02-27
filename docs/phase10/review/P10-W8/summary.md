# P10-W8 Review: Mutation Flows and Persistence Integrity

## Scope
Mutation flows, persistence roundtrip verification, and read-after-write
consistency for all disk-backed data structures (PagedArray, PagedImage,
SubImage write-through, TempImage masks, copy-on-write isolation).

## Key Deliverables
- `tests/mutation_persist_test.cpp` — 54 assertions across 12 test functions
- Writable open mode for `PagedArray` and `PagedImage` (`bool writable` param)
- TSM writer extensions: `write_double_cell`, `write_complex_cell`,
  read-back methods (`read_float_cell`, `read_double_cell`, `read_raw_cell`,
  `has_column`, `find_column`)
- `Table::open_rw()` TSM support — copies existing TSM data into writer
- Read-after-write consistency — read methods check TSM writer buffer first

## Test Coverage (54 assertions)
1. PagedArray float roundtrip (create, write, flush, reopen, verify)
2. PagedArray double roundtrip
3. PagedArray put_at / put_slice persistence
4. PagedImage full lifecycle (metadata + pixels)
5. PagedImage pixel update via re-open writable
6. PagedImage slice update
7. SubImage write-through to parent, persisted after flush
8. TempImage mask mutation
9. Copy-on-write isolation
10. Read-only error handling (SubLattice, SubImage)
11. Expression result write-back to image
12. LatticeIterator write cursor

## Regression
75/75 tests pass (0 failures).

## Bugs Found and Fixed
- **TSM write gap**: `Table::write_array_double_cell` and `write_array_complex_cell`
  only had SSM paths; TSM-backed tables threw "Table: not writable"
- **`Table::open_rw()` TSM blind spot**: Only set up SSM writers; TSM columns
  got no writer, so re-opened writable tables couldn't write
- **Read-after-write stale reads**: Sequential `put_at` calls read from disk
  TSM reader, losing prior in-memory modifications; fixed by checking TSM
  writer buffer first in read methods
