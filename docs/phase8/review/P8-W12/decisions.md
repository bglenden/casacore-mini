# P8-W12 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **Phase 9 entry conditions** — Phase 9 may begin when: all 24 interop
   matrix cells pass, all unit tests pass with no regressions, known
   differences are documented with explicit tolerances, and the exit report
   is complete and reviewed. These conditions are satisfied as of P8 closeout.

2. **Documented carry-forward items** — The following items are explicitly
   deferred from Phase 8 to Phase 9 or later:
   - Cross-frame frequency/radial_velocity conversion (LSRK<->BARY etc.)
   - Full IGRF model for MEarthMagnetic
   - SIP/TPV distortion support in FITSCoordinateUtil
   - UVW machine (was in original W4 scope, deferred due to complexity)

## Tradeoffs accepted

1. Retrospective review packet generation means the packets reflect the
   final state of each wave rather than the incremental state at the time
   of wave completion. Accepted because the implementation artifacts and
   decisions are accurately represented, and the retrospective nature is
   documented.
