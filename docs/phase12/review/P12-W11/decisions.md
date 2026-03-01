# P12-W11 Decisions

1. **Separate header/source pairs per component**: RefTable, ConcatTable, TableIterator,
   and ColumnsIndex each get their own header and source file rather than being combined
   into `table.hpp`. This keeps compilation units focused and avoids bloating table.hpp.

2. **CellValue comparison via visitor**: Since `std::variant` does not provide `operator<`
   by default, implemented `cell_compare()` using `std::visit` with special handling for
   complex types (real-then-imag ordering). Kept as file-local rather than public API.

3. **RefTable as non-virtual**: RefTable does not inherit from Table (it is not a
   polymorphic type). This avoids vtable overhead and matches the pattern where
   RefTable is always used directly, not through a base pointer.

4. **ConcatTable decompose_row uses linear scan**: For small table counts (typical), a
   backward linear scan is simpler than binary search. Could be optimized later if
   needed for many component tables.
