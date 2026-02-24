# P7-W4 Decisions

## D1: Writer always produces v2 format

The serializer always writes table_version=2 even when reading v1 input.
Version 2 omits per-column keywords and per-column storage manager assignments,
relying instead on the post-TD sentinel+SM-descriptor section. This simplifies
the writer and matches modern casacore behavior.

## D2: Fixture-based interop testing

Rather than synthesizing table.dat files via the casacore Table API (which
requires full table directories with SM data files), the interop matrix uses
existing corpus fixtures (`table_dat_ttable2_v0` and `table_dat_ttable2_v1`).
The casacore side copies the fixture; the mini side parses and re-serializes it.
This avoids the complexity of creating complete table directories at test time.

## D3: Casacore-side uses TableDesc::getFile for reading

The casacore interop tool deserializes column descriptors via
`casacore::TableDesc::getFile()` rather than manual raw AipsIO parsing. This
avoids the fragile requirement of reading every byte within each column
descriptor object (casacore's `AipsIO::getend()` throws if unread bytes remain).

## D4: Canonical format omits SM/setup lines

The canonical dump format for table_dat_full omits storage manager setup and
per-column SM assignment lines. These differ structurally between v1 and v2
formats, making cross-version comparison impractical. The canonical format
focuses on the Table header and column descriptor metadata, which is the
primary validation target.
