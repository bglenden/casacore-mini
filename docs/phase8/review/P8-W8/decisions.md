# P8-W8 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **Vector-of-unique_ptr composition** — CoordinateSystem stores its
   constituent Coordinate objects as `std::vector<std::unique_ptr<Coordinate>>`.
   This provides ownership semantics, polymorphic storage, and efficient
   move-only behavior without shared ownership complexity.

2. **Axis mapping with world/pixel index arrays** — Global world and pixel
   axis indices are mapped to per-coordinate local axis indices through
   explicit index arrays. This allows each Coordinate to manage its own
   local axis numbering while CoordinateSystem provides the global view.
   The mapping supports removed axes (e.g., when a Stokes axis is dropped)
   by using -1 sentinels for unmapped axes.

## Tradeoffs accepted

1. Vector-of-unique_ptr means CoordinateSystem is move-only (not copyable
   without explicit deep copy). Accepted because coordinate systems are
   typically created once and passed by reference, and deep copy is
   available via record round-trip if needed.

2. Axis mapping arrays add bookkeeping complexity. Accepted because the
   alternative (requiring all coordinates to use global axis numbering)
   would break the abstraction boundary of individual coordinate types.
