# P12-W11: Table infrastructure tranche B — Summary

## Scope
Implemented four table infrastructure components matching casacore upstream patterns:
- **RefTable**: Row-mapped view of a base table
- **ConcatTable**: Virtual concatenation of multiple tables with same schema
- **TableIterator**: Group-by iteration over key columns
- **ColumnsIndex**: In-memory index on scalar columns for fast key lookup

## Implementation

### RefTable
- Three constructors: explicit row indices, boolean mask, empty (progressive population)
- All cell reads delegate to base table through row-index mapping
- Provides `add_row()` for progressive building and `base_table()` access

### ConcatTable
- Takes vector of Table pointers; validates non-empty
- Builds cumulative row offset table for O(n) row decomposition
- Delegates reads to correct component table via `decompose_row()`
- Columns/keywords taken from first table

### TableIterator
- Sorts all row indices by key column values using custom `cell_compare()`
- `next()` advances to next group of rows with equal key values
- Each group exposed as a `RefTable`
- Supports `reset()` for re-iteration

### ColumnsIndex
- Builds `std::map<IndexKey, vector<uint64_t>>` with custom comparator
- Supports single-key and multi-key lookups
- `get_row_number()` enforces uniqueness
- `rebuild()` for refreshing after table mutations

### CellValue comparison
Both `table_iterator.cpp` and `columns_index.cpp` share a `cell_compare()` helper
(file-local in each) that handles variant index comparison, complex types
(real-then-imag ordering), and standard `operator<` for all other types.

## Test coverage
- `ref_table_test.cpp`: 24 checks (row indices, bool mask, empty ctor, out-of-range, base_table, delegation)
- `concat_table_test.cpp`: 21 checks (basic concat, decompose_row, out-of-range, empty throws, table_at, columns/keywords)
- `table_iter_test.cpp`: 20 checks (single/two-column grouping, content verification, reset, error cases)
- `columns_index_test.cpp`: 19 checks (single/two-column lookup, unique_key_count, contains, get_row_number, missing column, rebuild)
- Total: 84 new checks; all 107 tests pass

## Wave gate
10/10 checks pass.
