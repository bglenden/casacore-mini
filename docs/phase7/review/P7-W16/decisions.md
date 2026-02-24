# P7-W16 Decisions

## Autonomous decisions made

1. TiledShapeStMan uses _TSM1 data file suffix (not _TSM0). Reader probes
   both suffixes with fallback for compatibility.
2. TiledDataStMan hypercube is defined via accessor pattern matching casacore's
   TiledDataStManAccessor API.

## Assumptions adopted

1. TiledShapeStMan writes TiledStMan sub-object first (no preceding IPosition),
   unlike TiledColumnStMan and TiledCellStMan.
2. First non-dummy cube in TiledShapeStMan headers has file_nr >= 0 and
   non-empty tile_shape.

## Tradeoffs accepted

1. Header parser branches on SM type string to handle format differences.
   This couples the parser to specific manager names but is the simplest
   correct approach.
