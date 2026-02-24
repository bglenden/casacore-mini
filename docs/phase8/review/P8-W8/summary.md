# P8-W8 Summary

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Scope implemented

- CoordinateSystem class (`coordinate_system.hpp/cpp`) composing multiple
  Coordinate objects into a unified multi-axis system with world/pixel axis
  mapping.
- Vector-of-unique_ptr composition for heterogeneous coordinate storage.
- World and pixel index arrays mapping global axes to per-coordinate local axes.
- Record serialization for CoordinateSystem (to_record/from_record).
- Tests: coordinate_system_test.

## Public API changes

- `coordinate_system.hpp`: `CoordinateSystem` class with `add_coordinate()`,
  `n_coordinates()`, `coordinate(i)`, `n_world_axes()`, `n_pixel_axes()`,
  `world_axis_to_coordinate()`, `pixel_axis_to_coordinate()`, `to_world()`,
  `to_pixel()`, `to_record()`, `from_record()`.

## Behavioral changes

- Images can now have a full coordinate system with direction, spectral,
  Stokes, and linear axes composed together with proper axis mapping.

## Deferred items

None.
