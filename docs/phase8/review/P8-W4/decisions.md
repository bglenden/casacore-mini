# P8-W4 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **VelocityMachine for Doppler interconversion** — A dedicated
   `VelocityMachine` class handles conversion between frequency and radial
   velocity using the three standard Doppler conventions (radio, optical,
   relativistic). This mirrors casacore's `VelocityMachine` but with a
   simplified interface.

2. **Spherical round-trip for XYZ frame rotations** — Frame rotations for
   direction and position measures convert to spherical coordinates, apply the
   rotation, and convert back. This avoids accumulating floating-point error
   from direct Cartesian matrix multiplication and ensures unit-sphere
   normalization is preserved.

3. **Cross-frame freq/radvel deferred to P9** — Converting MFrequency or
   MRadialVelocity between reference frames (e.g., LSRK to BARY) requires
   the source direction and full velocity projection. This complex
   calculation is deferred to Phase 9 to keep Phase 8 focused on the
   core conversion infrastructure.

## Tradeoffs accepted

1. Deferring cross-frame frequency conversion means Phase 8 interop testing
   cannot exercise LSRK<->BARY frequency paths. Accepted because the core
   Doppler convention machinery is fully testable and the frame-dependent
   paths require additional infrastructure.
