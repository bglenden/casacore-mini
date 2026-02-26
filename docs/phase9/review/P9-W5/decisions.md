# P9-W5 Decisions

## Autonomous decisions made

1. Used batch-write model (collect all rows, flush once) rather than incremental
   row appends, matching SM writer API constraints.
2. FK validation checks 6 ID columns: ANTENNA1, ANTENNA2, FIELD_ID,
   DATA_DESC_ID, OBSERVATION_ID, STATE_ID. PROCESSOR_ID validation deferred
   since PROCESSOR subtable rows are not yet commonly populated.
3. Row structs use designated-initializer-friendly design (all fields have defaults).
4. flush() calls validate_foreign_keys() automatically before writing.

## Assumptions adopted

1. All TSM array columns use the same cell shape [3] within TiledColumnStMan.
2. Subtable array columns (POSITION, OFFSET, etc.) are skipped during SSM writes
   since SsmWriter only supports scalar cells.

## Tradeoffs accepted

1. Array data in main table limited to shape [3] per cell for all columns.
   Real MS files with variable-shape arrays need TiledShapeStMan or separate
   TSMs, which is a future enhancement.
