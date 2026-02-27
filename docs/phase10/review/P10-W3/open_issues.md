# P10-W3 Open Issues

- PagedArray persistence test deferred to W8 (mutation integrity wave)
  since it requires a working create → open → read roundtrip with
  TiledShapeStMan, which depends on the Table::create options being
  fully wired. ArrayLattice covers the core model and traversal
  semantics tested here.
- LatticeConcat and RebinLattice not yet implemented; scheduled for
  W9 (utility layer).
