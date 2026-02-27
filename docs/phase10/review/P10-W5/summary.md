# P10-W5 Summary: Region and Mask Persistence

## Overview
Implemented the full LC (lattice-coordinate) and WC (world-coordinate) region
hierarchies with serialization, mask computation, and coordinate-system-driven
WC→LC conversion.

## Deliverables

### Lattice-Coordinate (LC) Regions
- **lattice_region.hpp/cpp**: Complete LC region hierarchy:
  - `LcRegion` abstract base with bounding box, mask, serialization
  - `LcBox` — rectangular region (no mask)
  - `LcPixelSet` — explicit boolean mask over bounding box
  - `LcEllipsoid` — N-dimensional ellipsoid with computed mask
  - `LcPolygon` — 2D polygon with ray-casting point-in-polygon mask
  - `LcMask` — full-lattice temporary mask
  - `LcPagedMask` — persistent mask stub
  - `LcUnion`, `LcIntersection`, `LcDifference`, `LcComplement`, `LcExtension` — compound regions
  - `LatticeRegion` — holder wrapping LcRegion + optional Slicer
- **lattice_region_test.cpp**: 77 assertions covering all LC region types,
  serialization roundtrips, mask computation, and from_record dispatch.

### World-Coordinate (WC) Regions
- **image_region.hpp/cpp**: Complete WC region hierarchy:
  - `WcRegion` abstract base with `to_lc_region(cs, shape)` conversion
  - `WcBox` — rectangular region in world coordinates
  - `WcEllipsoid` — ellipsoidal region in world coordinates
  - `WcPolygon` — 2D polygon in world coordinates
  - `WcUnion`, `WcIntersection`, `WcDifference`, `WcComplement` — compound WC regions
  - `ImageRegion` — unified container (LcRegion | WcRegion | Slicer)
  - `RegionHandler` — named region management (define/get/remove/rename)
  - `RegionManager` — factory for creating common region types
- **image_region_test.cpp**: 98 assertions covering all WC region types,
  WC→LC conversion via CoordinateSystem, ImageRegion roundtrips,
  RegionHandler CRUD, and RegionManager factories.

## Test Results
- lattice_region_test: 77 passed, 0 failed
- image_region_test: 98 passed, 0 failed
- Full regression: 72/72 tests passed

## Key Design Decisions
- WC regions store axis names/units for coordinate-system-independent definition
- WC→LC conversion uses `find_pixel_axis_by_name` + `world_to_pixel_1d` for
  per-axis coordinate transformation via the CoordinateSystem
- `LatticeArray<bool>` mutations use `put(IPosition, value)` not `mutable_data()`
  due to `vector<bool>` lacking `.data()`
- LcEllipsoid/LcPolygon compute bounding boxes in constructor body after
  setting up dummy Slicer in initializer (Slicer has no default ctor)
