# P9-W11 Open Issues

1. **mini→casacore failures (5/5)**: casacore cannot open MS directories
   produced by casacore-mini. Root cause: casacore expects `table.f1` files
   that casacore-mini's SSM writer does not create in the expected layout.
   This is a table-format-level interop gap (not an MS-API-level issue).

2. **casacore→mini failures (2/5)**: casacore-mini cannot verify
   ms-representative and ms-concat artifacts produced by casacore. Likely
   due to casacore using IncrementalStMan or other data managers that
   casacore-mini's MS column wrappers don't auto-detect.
