# P8-W10 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **6 artifacts x 4 directions = 24 cells** — The interop matrix tests 6
   artifact types (Measure record, CoordinateSystem record, FITS header,
   TableMeasure column, Direction conversion result, Spectral conversion
   result) across 4 directions (casacore-write/mini-read, mini-write/
   casacore-read, casacore-write/casacore-read, mini-write/mini-read).
   All 24 cells must pass for the phase gate to be satisfied.

2. **Semantic verification for coordinate artifacts** — Coordinate artifacts
   are verified by checking that round-trip pixel<->world transforms agree
   within documented tolerances, rather than requiring byte-level record
   equality. This accommodates legitimate differences in optional record
   fields between implementations while ensuring functional equivalence.

## Tradeoffs accepted

1. Semantic verification is more complex to implement than byte-level
   comparison but is necessary because casacore and casacore-mini may
   represent the same coordinate system with different optional fields
   in their record representations.
