# P7-W16 Open Issues

## Blocking issues

None.

## Non-blocking issues

1. TiledShapeStMan header parsing requires special handling: no pre-TiledStMan
   IPosition (unlike TiledColumnStMan/TiledCellStMan). This difference is
   handled by SM type check in the parser.
2. TiledDataStMan requires defineHypercolumn on the TableDesc. The casacore
   writer handles this; mini writer mirrors the structure.

## Proposed follow-ups

1. W17: Full-table validation hardening across all managers.
