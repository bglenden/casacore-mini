# P7-W8 Decisions

## Decision 1: Add ISM as dedicated directory matrix case
**Choice**: Added IncrementalStMan as a separate `ism_dir` matrix case rather
than relying solely on the existing `table_dir` case (which uses SSM).

**Rationale**: The plan requires all six storage manager types to be covered in
the matrix. While the `table_dir` case covers SSM, ISM was missing. Adding a
dedicated ISM case with explicit IncrementalStMan binding ensures both core
managers have matrix coverage.

## Decision 2: Reuse verify_tiled_dir_artifact pattern for ISM
**Choice**: ISM verification on the casacore side reuses `verify_tiled_dir_artifact`
(which compares canonical metadata lines with SM file details stripped).

**Rationale**: The structural comparison pattern is identical across all
directory-level cases — compare table metadata (row count, column names/types)
while wildcarding SM file counts. No need for a separate verify function.

## Decision 3: W8 gate validates matrix completeness
**Choice**: The `check_phase7_w8.sh` gate script verifies that all six required
SM case names appear in the matrix runner, rather than re-running the matrix.

**Rationale**: Running the full matrix requires casacore installed, which is not
available in all CI environments. The gate validates structural completeness
(all cases present) while the matrix runner validates functional correctness
(interop actually works) when casacore is available.
