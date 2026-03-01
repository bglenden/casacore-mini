# P12-W12 Decisions

1. **TSM mutation tested via TiledColumnStMan only**: All four TSM variants
   share the same code path in `Table::add_row()` and `Table::remove_row()`.
   Testing TiledColumnStMan exercises the shared TSM writer setup/copy/flush
   logic. Separate tests for TiledCell/Shape/DataStMan would only test the
   tile layout computation, which is already covered by `tiled_directory_test`.

2. **Row-count ordering fix**: Changed `Table::add_row()` to defer the row-count
   update to after writer rebuild. This ensures TSM readers see the old row count
   during the data copy phase. The `remove_row()` method already had the correct
   ordering (comment: "Re-read table.dat at old row count still").

3. **Deferred managers excluded from Phase 12**: MemoryStMan, Adios2StMan,
   Dysco, and AlternateMans require concrete demand evidence to promote.
   No such evidence has been presented during Phase 12.
