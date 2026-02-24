# P7-W4 Summary

## Scope implemented

Full table.dat body interoperability: parse and serialize the complete table.dat
file including TableDesc, column descriptors, and storage manager metadata.

Deliverables:

- `include/casacore_mini/table_desc.hpp`: types for full table.dat body
  (`DataType`, `ColumnKind`, `ColumnDesc`, `StorageManagerSetup`,
  `ColumnManagerSetup`, `TableDesc`, `TableDatFull`) and parser declarations.
- `src/table_desc.cpp`: full table.dat parser supporting v1 and v2 formats,
  including polymorphic column descriptors (`ScalarColumnDesc<T>`,
  `ArrayColumnDesc<T>`), TableRecords, and post-TD storage manager assignments.
- `include/casacore_mini/table_desc_writer.hpp` + `src/table_desc_writer.cpp`:
  serializer that always writes v2 format.
- `tests/table_desc_test.cpp`: fixture-based tests for v0, v1, roundtrip, and
  malformed-input rejection at every truncation point.
- Interop tool extensions: `write-table-dat-full`, `verify-table-dat-full`,
  `dump-table-dat-full` subcommands in both `interop_mini_tool.cpp` and
  `casacore_interop_tool.cpp`.
- `tools/interop/run_phase7_matrix.sh`: `table_dat_full` matrix case passes
  all 4 cells (casacore->casacore, casacore->mini, mini->mini, mini->casacore).
- `tools/check_phase7_w4.sh`: wave gate script wired into CI.

## Public API changes

- Added `casacore_mini::parse_table_dat_full()` and
  `casacore_mini::read_table_dat_full()` in `table_desc.hpp`.
- Added `casacore_mini::serialize_table_dat_full()` and
  `casacore_mini::write_table_dat_full()` in `table_desc_writer.hpp`.
- Added types: `DataType`, `ColumnKind`, `ColumnDesc`, `StorageManagerSetup`,
  `ColumnManagerSetup`, `TableDesc`, `TableDatFull`.

## Behavioral changes

- No existing behavior modified. New parsing/serialization functions operate
  on the full table.dat body; existing header-only parsing is unchanged.

## Deferred items

- v0 (pre-TableRecord) table.dat format is parsed but not round-tripped
  through the writer (writer always produces v2).
