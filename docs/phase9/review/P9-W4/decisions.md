# P9-W4 Decisions

## Autonomous decisions made

1. Used lazy-open pattern for SM readers (opened on first cell access) to avoid
   overhead when only a subset of columns is needed.
2. For TiledColumnStMan testing, forced all variable-shape array columns to
   uniform shape [3] since TiledColumnStMan requires homogeneous cell shapes.
3. Provided 7 column wrapper classes covering the main table and 6 most-used
   subtables (ANTENNA, SPECTRAL_WINDOW, FIELD, DATA_DESCRIPTION, POLARIZATION,
   OBSERVATION).
4. Used `read_scalar_measure()` and `read_array_measures()` from the existing
   table_measure_column infrastructure for TIME and UVW measure access.

## Assumptions adopted

1. Subtable array columns stored in SSM cannot be read yet (SsmReader only
   supports scalar cell reads); these return empty vectors.
2. MsMainColumns UVW reading uses `read_raw_cell()` + memcpy for tp_double
   arrays since TiledStManReader has no `read_double_cell()` method.

## Tradeoffs accepted

1. Not all 17 subtable types have wrapper classes yet — only the 6 most
   commonly used. Remaining subtables can be added as needed in later waves.
