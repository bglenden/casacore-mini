# P8-W4 Open Issues

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Blocking issues

None.

## Non-blocking issues

1. **Cross-frame frequency/radial_velocity conversion deferred to P9** —
   Converting MFrequency or MRadialVelocity between reference frames (e.g.,
   LSRK to BARY) requires direction-dependent Doppler shift calculations.
   This is deferred to Phase 9. The current implementation handles Doppler
   convention interconversion (radio/optical/relativistic) but not frame
   changes for these types.
