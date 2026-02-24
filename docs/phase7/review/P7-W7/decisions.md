# P7-W7 Decisions

## Decision 1: Directory-level interop for all four tiled managers
**Choice**: Implement structural (directory-level) interop for all four tiled
managers rather than attempting full bucket-level read/write for a subset.

**Rationale**: Consistent with W6 pragmatic approach. All four tiled SMs are
now covered in the interop matrix at the same level of coverage as SSM and ISM.
Full bucket-level implementations are deferred uniformly.

## Decision 2: Accept TableRecord as nested record type
**Choice**: Modified `read_aipsio_record_impl` to accept both `Record` and
`TableRecord` object headers since the body format is identical.

**Rationale**: TiledDataStMan tables require `defineHypercolumn` which stores
hypercolumn metadata as nested `TableRecord` objects in the table's private
keywords. Without this fix, mini cannot parse any casacore table that uses
`defineHypercolumn`. This is a genuine parser capability gap, not specific to
tiled managers.

## Decision 3: Cover all four tiled managers (not just two)
**Choice**: The original plan suggested implementing TiledColumnStMan and
TiledCellStMan while deferring TiledShapeStMan and TiledDataStMan. We chose
to cover all four at the directory level.

**Rationale**: Since directory-level interop requires no bucket parsing, the
incremental cost per SM type is minimal (just the casacore table creation code
in the interop tool). Covering all four provides better corpus evidence and
reduces risk of discovering incompatibilities late.
