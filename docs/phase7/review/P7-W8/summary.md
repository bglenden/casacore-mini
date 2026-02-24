# P7-W8 Summary: Full Directory Matrix Expansion

## Scope Implemented

Expanded the interop matrix to include all six required storage manager types
at the directory level:

1. **StandardStMan** (`table_dir`) — already present from W6
2. **IncrementalStMan** (`ism_dir`) — new in W8
3. **TiledColumnStMan** (`tiled_col_dir`) — already present from W7
4. **TiledCellStMan** (`tiled_cell_dir`) — already present from W7
5. **TiledShapeStMan** (`tiled_shape_dir`) — already present from W7
6. **TiledDataStMan** (`tiled_data_dir`) — already present from W7

Each case runs the full 2×2 producer/consumer matrix:
- casacore produces → casacore verifies ✓
- casacore produces → mini verifies ✓
- mini produces → mini verifies ✓
- mini produces → casacore verifies → EXPECTED_FAIL (no SM data files)

## Public API Changes

None. All changes are in interop tooling and test infrastructure.

## Behavior Deltas

- `interop_mini_tool` now supports `write-ism-dir` and `verify-ism-dir` subcommands
- `casacore_interop_tool` now supports `write-ism-dir` and `verify-ism-dir` subcommands
- `run_phase7_matrix.sh` now includes `ism_dir` case alongside the existing 5 cases
- W8 wave gate validates all six required SM cases are present in the matrix
