# P8-W4 Summary

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Scope implemented

- VelocityMachine (`velocity_machine.hpp/cpp`) for Doppler convention
  interconversion (radio, optical, relativistic) at a given rest frequency.
- Extended measure conversion engine with remaining measure types: MFrequency,
  MRadialVelocity, MDoppler, MEarthMagnetic.
- Spherical round-trip for XYZ frame rotations in direction and position
  conversions.
- Tests: velocity_machine_test, measure_convert_full_test.

## Public API changes

- `velocity_machine.hpp`: `VelocityMachine` class with `to_velocity()`,
  `to_frequency()`, and Doppler convention conversion methods.
- Extended `convert_measure()` to handle MFrequency, MRadialVelocity,
  MDoppler, and MEarthMagnetic types.

## Behavioral changes

- VelocityMachine converts between frequency and radial velocity using the
  standard Doppler formulas for radio, optical, and relativistic conventions.
- Spherical round-trip ensures XYZ frame rotations preserve unit-sphere
  normalization.

## Deferred items

- Cross-frame frequency/radial_velocity conversion (e.g., LSRK frequency to
  BARY frequency) deferred to Phase 9. Current implementation handles Doppler
  convention changes but not reference frame changes for these types.
