# P10-W5 Open Issues

- WCLELMask (dynamic LEL-based masks) deferred to W6/W7 when the expression
  engine is available.
- LcPagedMask is a stub returning all-true masks. Full persistent mask
  support requires table column integration (deferred to W8).
- Compound WC region serialization (WcUnion, WcIntersection, etc.) serializes
  to Record but from_record dispatch does not yet support compound types
  (only WcBox, WcEllipsoid, WcPolygon are dispatched). Add when needed.
- WcPolygon::to_lc_region assumes pixel axes 0 and 1 map to the x/y axis
  names, which works for standard 2D images but may need generalization for
  transposed axis orders.
- RegionHandler persistence is Record-based; integration with image table
  keywords for persistent named regions is a W8 task.
