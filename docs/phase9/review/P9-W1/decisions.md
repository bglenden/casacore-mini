# P9-W1 Decisions

## Autonomous decisions made

1. Used composition-over-inheritance for MS design (MeasurementSet wraps
   TableDirectory rather than inheriting from MSTable template hierarchy).
2. Mapped casacore's per-subtable `MS*` classes to enum + factory function
   pairs rather than individual classes, reducing boilerplate.
3. Consolidated MSReader/MSDerivedValues/MSKeys into MsMetaData and
   MSLister into MsSummary to minimize API surface.

## Assumptions adopted

1. casacore's MS2 format is the target (not MS1 or MS3).
2. Only 12 required subtables need to be created by default; optional
   subtables (DOPPLER, FREQ_OFFSET, SOURCE, SYSCAL, WEATHER, POINTING)
   are created on demand.

## Tradeoffs accepted

1. Simplified selection expression syntax vs full TaQL compatibility.
   Covers the 8 required categories with casacore-compatible subset syntax.
