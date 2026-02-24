# Phase 7 Wave 7 Review Packet

## Scope
Tiled storage manager interoperability: structural (directory-level) interop
for all four tiled managers (TiledColumnStMan, TiledCellStMan, TiledShapeStMan,
TiledDataStMan). Full bucket-level cell data read/write deferred with explicit
documentation.

## Deliverables

### New Files
- `tests/tiled_directory_test.cpp` -- 4 tests (tiled col roundtrip, tiled cell
  roundtrip, shape+data roundtrip, pagedimage fixture read)
- `tools/check_phase7_w7.sh` -- Wave gate script
- `docs/phase7/deferred_managers.md` -- Deferral documentation for all six SM
  bucket-level implementations with rationale and carry-forward plan

### Modified Files
- `src/interop_mini_tool.cpp` -- Added write/verify/dump for tiled-col-dir,
  tiled-cell-dir, tiled-shape-dir, tiled-data-dir; added strip_sm_file_lines
  and expected metadata builders for all four tiled types
- `tools/interop/casacore_interop_tool.cpp` -- Added TiledColumnStMan,
  TiledCellStMan, TiledShapeStMan, TiledDataStMan table creation using casacore
  APIs; added verify/dump using strip_sm_file_lines approach
- `tools/interop/run_phase7_matrix.sh` -- Added 4 tiled directory matrix cases
- `src/aipsio_record_reader.cpp` -- **Bug fix**: Accept `TableRecord` as nested
  record object type (not just `Record`); tables with hypercolumn definitions
  (TiledDataStMan) store TableRecord objects in private keywords
- `CMakeLists.txt` -- Added tiled_directory_test target
- `tools/check_ci_build_lint_test_coverage.sh` -- Added W7 gate

## Interop Matrix Results
- casacore -> mini: PASS (all 4 tiled types)
- casacore -> casacore: PASS (all 4 tiled types)
- mini -> mini: PASS (all 4 tiled types)
- mini -> casacore: Expected fail (mini doesn't write SM data files)

## Bug Fix Detail
The AipsIO record reader (`read_aipsio_record_impl`) only accepted objects with
type `"Record"`. Tables created with `defineHypercolumn` (required by
TiledDataStMan) store hypercolumn metadata as nested `TableRecord` objects in
the table's private keywords. The fix accepts both `Record` and `TableRecord`
since the body format (RecordDesc + recordType + field values) is identical.

## Verification
- `bash tools/check_format.sh` -- PASS
- `bash tools/check_ci_build_lint_test_coverage.sh build-ci-local` -- PASS (86% coverage)
- `bash tools/check_docs.sh build-ci-local-doc` -- PASS
- `bash tools/check_phase7_w7.sh build-ci-local` -- PASS
- `bash tools/interop/run_phase7_matrix.sh build-phase7` -- PASS

## Design Decision: Directory-Level Tiled Interop
Consistent with the W6 pragmatic approach, W7 implements directory-level
structural interop rather than full bucket-level tiled manager I/O.
Full bucket-level implementations are documented as deferred in
`docs/phase7/deferred_managers.md` with carry-forward plan.
