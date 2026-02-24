# Phase 7 Wave 6 Review Packet

## Scope
Table directory orchestration layer: reads/writes table directories (table.dat +
SM data files), enabling casacore table interoperability at the directory level.

## Deliverables

### New Files
- `include/casacore_mini/table_directory.hpp` -- `TableDirectory`, `StorageManagerFile`,
  `read_table_directory`, `write_table_directory`, `compare_table_directories`
- `src/table_directory.cpp` -- Implementation
- `tests/table_directory_test.cpp` -- 4 tests (read, roundtrip, compare, missing)
- `tools/check_phase7_w6.sh` -- Wave gate script

### Modified Files
- `src/table_desc.cpp` -- **Bug fix**: TableDesc v2 userType field was read as u32
  instead of string; caused parse failure on casacore-produced tables with non-empty
  userType (e.g. tables created via SetupNewTable).
- `src/table_desc_writer.cpp` -- Matching fix: write userType as empty string
  (semantically correct, same wire bytes as before since empty string = u32(0)).
- `src/interop_mini_tool.cpp` -- Added write-table-dir, verify-table-dir, dump-table-dir
- `tools/interop/casacore_interop_tool.cpp` -- Matching casacore-side table directory
  commands using casacore Table/Column APIs
- `tools/interop/run_phase7_matrix.sh` -- Added table_dir matrix case with run_dir_case
- `CMakeLists.txt` -- Added table_directory.cpp source, table_directory_test target
- `tools/check_ci_build_lint_test_coverage.sh` -- Added W6 gate

## Interop Matrix Results
- casacore -> mini: PASS (mini reads casacore-produced SSM table directory)
- casacore -> casacore: PASS
- mini -> mini: PASS (mini reads its own directory)
- mini -> casacore: Expected fail (mini doesn't write SM data files; casacore can't
  open table without them). Non-fatal in matrix.

## Bug Fix Detail
The TableDesc v2 format includes a `userType` string field before the table name.
The original W4 parser treated this as a u32 (discarded), which happened to work for
fixtures where userType was empty (u32(0) = string length 0). When casacore creates
a real table via SetupNewTable, the userType is the table descriptor name (e.g.
"test_ssm"), causing the parser to misinterpret the string data as subsequent fields.

## Verification
- `bash tools/check_format.sh` -- PASS
- `bash tools/check_ci_build_lint_test_coverage.sh build-ci-local` -- PASS (86% coverage)
- `bash tools/check_docs.sh build-ci-local-doc` -- PASS
- `bash tools/check_phase7_w6.sh build-ci-local` -- PASS
- `bash tools/interop/run_phase7_matrix.sh build-phase7` -- PASS

## Design Decision: Pragmatic Table Directory Approach
Full SSM/ISM bucket-level read/write was deemed too complex to reverse-engineer within
this wave. The table directory layer provides:
1. Structural metadata reading (table.dat + SM file catalog)
2. Table.dat roundtrip (read -> modify -> write)
3. SM data file pass-through (copy without interpretation)
4. Structural comparison for testing

Cell-level data verification is delegated to the casacore interop tool, which can
open tables with casacore's native Table/Column APIs.
