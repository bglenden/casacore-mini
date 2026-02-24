# P8-W2 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **Variant-based measure value model** — Measure values are represented as a
   `std::variant` of 9 value structs rather than an inheritance hierarchy. The 9
   measure types form a closed set, making variant the natural representation
   with full pattern-matching support and no heap allocation.

2. **MeasureRefType as variant of enums** — Each measure type has its own
   reference frame enum (e.g., `EpochRef`, `DirectionRef`). `MeasureRefType` is
   a `std::variant` of these enums, allowing type-safe frame handling without
   runtime string comparisons.

3. **CamelCase type aliases** — `MeasureValue`, `MeasureRefType`, and related
   aliases use CamelCase to match the existing `CellValue` convention in the
   codebase.

4. **Upstream synonym support** — String-to-enum parsing recognizes casacore
   synonyms (IAT=TAI, TT/ET=TDT, UT=UT1, GMST=GMST1, LSR=LSRK, OPTICAL=Z,
   RELATIVISTIC=BETA, AZELNE=AZEL, AZELNEGEO=AZELGEO) for backward
   compatibility with existing table metadata.

## Tradeoffs accepted

1. Variant-based model requires visitor/switch for type dispatch, which is more
   verbose than virtual dispatch but avoids heap allocation and provides
   exhaustiveness checking at compile time.
