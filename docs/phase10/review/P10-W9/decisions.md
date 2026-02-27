# P10-W9 Design Decisions

## 1. Read-only semantics for LatticeConcat and RebinLattice
Both types are read-only views. Writes throw at runtime rather than being
prevented at compile time, matching the existing SubLattice pattern.

## 2. ImageConcat delegates to LatticeConcat
Rather than duplicating concatenation logic, ImageConcat wraps a
LatticeConcat internally and adds image metadata (CS, units, info)
from the first component image.

## 3. ImageBeamSet serialization uses positional keys
Beams are stored in Record as "beam_C_S" keys (channel_stokes),
matching upstream casacore's per-plane beam persistence format.

## 4. Nearest-neighbor regridding only
`image_regrid()` uses nearest-neighbor interpolation for simplicity.
Full bilinear/cubic interpolation can be added in a future wave if
needed, but nearest-neighbor is sufficient for the API parity target.

## 5. CoordinateSystem::to_world/to_pixel scatter-gather
Each coordinate's pixel/world axes are mapped through world_map and
pixel_map arrays. Replacement values fill in for removed axes.
This matches the upstream casacore axis-mapping architecture.
