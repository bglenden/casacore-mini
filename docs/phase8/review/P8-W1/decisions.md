# P8-W1 Decisions

## Autonomous decisions made

1. **ERFA detection via pkg-config + vendored fallback** — ERFA is not in Homebrew;
   pkg-config is the primary detection method (works after `meson install`). Vendored
   `third_party/erfa/erfa.c` + `erfa.h` is the fallback (compiled as static C library
   with warnings suppressed).

2. **WCSLIB via pkg-config only, no vendored fallback** — WCSLIB is available in
   Homebrew (`brew install wcslib`) and Linux package managers. Vendoring would
   require ~10 C files plus build configuration; not worthwhile given availability.

3. **Both dependencies are FATAL_ERROR if missing** — rather than soft/optional,
   since all Phase 8 code needs them. Clear install instructions in the error message.

4. **Variant-based measures, inheritance-based coordinates** — as specified in the
   plan. Measures are a closed set of 9 types (natural variant). Coordinates have
   genuinely polymorphic interfaces (abstract base + unique_ptr).

## Assumptions adopted

1. ERFA 2.0.1 provides sufficient IAU standard coverage for all in-scope conversions.
2. WCSLIB 7.0+ provides all ~30 projection types needed by DirectionCoordinate.
3. Original casacore uses SOFA only for testing, not as a build dep — confirmed by
   reading casacore-original CMakeLists.txt and README.

## Tradeoffs accepted

1. Requiring meson/ninja to build ERFA from source on macOS adds a build step for
   new developers. Mitigated by vendored fallback option and clear README instructions.
