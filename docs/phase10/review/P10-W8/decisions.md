# P10-W8 Decisions

## D1: Read-after-write via writer buffer, not flush-and-reread
When a table is writable and has a TSM writer, read methods now check the
writer's in-memory buffer first. This avoids flushing to disk after every
write just to get read consistency, at the cost of a column-name lookup
per read call.

## D2: TSM writer read-back methods on TiledStManWriter
Added read_float_cell, read_double_cell, read_raw_cell, has_column, and
find_column to TiledStManWriter. These read directly from the in-memory
data buffer used by the writer, providing zero-copy read-after-write.

## D3: Table::open_rw copies TSM data into writer
When re-opening a TSM-backed table for writing, open_rw now creates a
TiledStManWriter, reads all existing cell data from the TSM reader via
read_raw_cell, and copies it into the writer. This allows subsequent
writes to modify the data and flush will write the complete updated state.

## D4: Flush still uses else-if chain
Table::flush uses SSM else-if ISM else-if TSM. For tables with both SSM
and TSM columns, only one writer gets flushed. This is acceptable for now
since PagedArray/PagedImage use TSM-only for pixels and keywords go through
table_dat directly. A future wave can address mixed-SM flush if needed.
