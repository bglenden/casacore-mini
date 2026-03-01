# P12-W14 Decisions

1. **W4 gate fix**: The `drop-table-executes` check grepped for `remove_all`
   which was never the implementation pattern — the actual code uses
   `Table::drop_table`. Corrected the gate to match the real implementation.

2. **Fresh build directory**: Used `build-closeout` as a clean build directory
   for final verification, separate from the per-wave `build-p12` directory
   used by individual wave gates.

3. **Interop matrix deferred**: Full interop matrix suites (Phase-8, Phase-10,
   Phase-11) require a casacore-enabled host and are documented as an open
   issue rather than blocking closeout.
