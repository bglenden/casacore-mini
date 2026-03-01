# P12-W11 Open Issues

1. **Duplicated `cell_compare()` helper**: Both `table_iterator.cpp` and `columns_index.cpp`
   have identical file-local `cell_compare()` functions. Could be extracted to a shared
   utility header, but keeping them local avoids introducing new public API surface.
   Low priority — revisit if a third consumer appears.

2. **ConcatTable does not validate schema compatibility**: The constructor accepts any
   list of tables without checking that column names/types match. This matches casacore
   upstream behavior (which also defers such checks) but could produce confusing errors.
