# P12-W2 Summary: Table Mutation Foundation

## Deliverables
- `Table::add_row(n)`: Extends a writable table by n default-initialized rows.
- `Table::remove_row(row)`: Removes a single row, shifting subsequent rows down.
- Both methods work for SSM, ISM, and TSM-backed tables.
- 12 tests covering scalar, array, ISM, first/last row removal, keywords
  preservation, and error cases.

## Implementation
Both methods use a flush-rebuild strategy:
1. Flush pending writes to disk
2. Update row_count in table_dat
3. Rebuild writers with new row count, copying existing cell data from readers
4. For remove_row, skip the removed row during copy

This ensures correctness across all storage manager types without modifying
their internal data structures.

## Test Coverage
- SSM scalar add/remove
- SSM fixed-shape array add/remove
- ISM scalar add/remove
- First row removal
- Last row removal
- Add zero rows (no-op)
- Out-of-range removal (throws)
- Read-only add (throws)
- Keywords preserved across mutation
