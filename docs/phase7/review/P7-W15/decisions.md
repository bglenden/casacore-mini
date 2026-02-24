# P7-W15 Decisions

## Autonomous decisions made

1. Implemented all four tiled managers (Column, Cell, Shape, Data) in a single
   module (`tiled_stman.cpp`) since they share the TSM header format, tile
   layout computation, and bucket structure.
2. TsmReader dispatches based on SM type string from the TSM header to handle
   differences in cube semantics.

## Assumptions adopted

1. Tile layout is derived from TSM header hypercube metadata (cube shape,
   tile shape, file number).
2. Data file suffix (_TSM0 or _TSM1) is determined from the TSM header's
   file sequence numbers.

## Tradeoffs accepted

1. Single module for all tiled managers increases file size but avoids code
   duplication for shared parsing infrastructure.
