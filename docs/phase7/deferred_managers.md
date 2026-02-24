# Storage Manager Implementations (Historical Deferral Record)

Date: 2026-02-24

## Current status

All 6 required storage managers are **fully implemented** as of Phase 7 W16.
This document is a historical record of the earlier (superseded) deferral
decision from the first Phase-7 pass.

No managers remain deferred. The carry-forward plan described below was
executed during the Phase-7 recovery waves (W12–W16).

## Implementation summary

| Manager | Read | Write | Cell verification | Wave |
|---|---|---|---|---|
| StandardStMan | Yes | Yes | 20 scalar + array cell values | W12/W13 |
| IncrementalStMan | Yes | Yes | 30 cell values (3 cols × 10 rows) | W14 |
| TiledColumnStMan | Yes | Yes | TSM cells verified | W15 |
| TiledCellStMan | Yes | Yes | TSM cells verified | W15 |
| TiledShapeStMan | Yes | Yes | TSM cells verified | W16 |
| TiledDataStMan | Yes | Yes | TSM cells verified | W16 |

Implementation files:
- `include/casacore_mini/standard_stman.hpp` / `src/standard_stman.cpp`
- `include/casacore_mini/incremental_stman.hpp` / `src/incremental_stman.cpp`
- `include/casacore_mini/tiled_stman.hpp` / `src/tiled_stman.cpp`

## Historical deferral (superseded)

The original Phase-7 pass (W1–W10) deferred all bucket-level cell data
read/write for the six required storage managers. The deferral rationale was
implementation complexity exceeding the original wave scope.

The reopened Phase-7 recovery (W11–W18) reversed this deferral policy:
no new deferrals were allowed for the required manager set.

## Policy reference

Current policy is in `docs/phase7/plan.md` (recovery waves W11+):

1. the six required managers are no longer eligible for Phase-7 deferral
2. Phase 7 is complete only after full cell-level interop for all six managers
3. see `docs/phase7/exit_report.md` for final evidence
