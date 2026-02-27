# P10-W5 Decisions

## LatticeArray<bool> mutation strategy
`vector<bool>` has no `.data()` method, so `LatticeArray<bool>::mutable_data()`
cannot work. All bool array mutations use `put(IPosition, value)` with
`delinearize()` for index conversion. This is slightly slower than direct
pointer access but type-safe and correct.

## Slicer initialization for deferred bounding boxes
LcEllipsoid and LcPolygon need to compute their bounding boxes from constructor
parameters, but must initialize the LcRegion base (which needs a Slicer) in the
initializer list. Solution: pass a dummy Slicer in the initializer, then call
`set_bounding_box()` in the constructor body.

## WC region axis identification by name
WC regions store axis names (e.g., "Right Ascension", "Declination") and use
`find_pixel_axis_by_name` to locate the corresponding pixel axis in whatever
CoordinateSystem they're applied to. This allows regions created with one CS to
be applied to another if axis names match.

## world_to_pixel_1d approach
Per-axis world-to-pixel conversion builds a full reference-value world vector,
replaces the target axis, and calls `Coordinate::to_pixel()`. This handles
non-linear projections correctly (e.g., SIN, TAN) rather than assuming a
simple linear transform.

## ImageRegion as a variant container
`ImageRegion` holds one of LcRegion, WcRegion, or Slicer. The `to_lc_region()`
method converts any of these to an LcRegion for actual pixel-domain access.
This mirrors casacore's ImageRegion design.

## RegionHandler as simple map
RegionHandler uses an `unordered_map<string, ImageRegion>` for named region
storage. This is simpler than casacore's approach (which ties into table
keywords) but sufficient for in-memory use. Serialization goes through Record.

## WCLELMask deferred
WCLELMask (dynamic masks from LEL expressions) is deferred to W6/W7 when the
expression engine is available.
