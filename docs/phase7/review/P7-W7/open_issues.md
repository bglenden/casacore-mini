# P7-W7 Open Issues

## Deferred: Bucket-level SM data read/write
All six storage manager bucket-level implementations are deferred.
See `docs/phase7/deferred_managers.md` for full rationale and carry-forward plan.

## pagedimage fixture lacks table.dat
The `data/corpus/fixtures/pagedimage_coords/` fixture only contains
`showtableinfo.txt` and `table_keywords.bin` -- no `table.dat` file. The
`test_read_pagedimage_fixture` test SKIPs when the fixture is not present.
Adding a full table.dat to this fixture would require generating it from a
casacore build tree.

## TiledDataStMan hypercolumn requirement
TiledDataStMan requires `defineHypercolumn` on the `TableDesc` before creating
a table, plus `TiledDataStManAccessor::addHypercube` for explicit hypercube
management. This is the most complex tiled API and the most fragile to set up
correctly. The interop test covers the write/read cycle successfully.
