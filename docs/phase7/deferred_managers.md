# Deferred Storage Manager Implementations

Date: 2026-02-24

## Scope

This document records the deferral of full bucket-level storage manager
implementations per the Phase 7 autonomy policy (section "deferral rules" in
`docs/phase7/plan.md`).

## What was implemented

Phase 7 implements **structural interoperability** at the table-directory level
for all six required storage managers:

| Manager | table.dat parse/write | Directory read | Interop matrix (casacore->mini) |
|---|---|---|---|
| StandardStMan | Yes | Yes | PASS |
| IncrementalStMan | Yes | Yes | PASS |
| TiledColumnStMan | Yes | Yes | PASS |
| TiledCellStMan | Yes | Yes | PASS |
| TiledShapeStMan | Yes | Yes | PASS |
| TiledDataStMan | Yes | Yes | PASS |

"Structural interoperability" means:
- table.dat containing any of these SMs is correctly parsed and re-serialized
- SM type names, sequence numbers, and column bindings roundtrip correctly
- Directory-level metadata (file presence, sizes, SM type associations) is read
- The interop matrix verifies casacore-produced directories with each SM type

## What was deferred

**Full bucket-level cell data read/write** for all six storage managers.

This means casacore-mini cannot currently:
- Read individual cell values from SM data files (table.f0, table.f1, ...)
- Write cell data into SM data files
- Open a table for random-access column reads the way casacore's Table API does

## Deferral rationale

### Complexity

Each storage manager has a distinct on-disk format with significant complexity:

1. **StandardStMan**: Bucket-based format with variable-length columns, row
   index tables, and multiple bucket types (data, index, free-list). The format
   is not documented and must be reverse-engineered from casacore source
   (`SSMBase.cc`, `SSMColumn.cc`, `SSMIndex.cc`, ~3000 lines).

2. **IncrementalStMan**: Delta-encoding with bucket chains, per-column delta
   encoding schemes, and a staircase row index. More complex than SSM due to
   the incremental storage model (~2500 lines in casacore source).

3. **TiledColumnStMan**: Hypercube tiling with bucket-based cache, tile
   positioning, and multi-dimensional data layout. Shares infrastructure with
   other tiled managers via `TiledStMan` base class (~4000 lines total).

4. **TiledCellStMan**: Similar to TiledColumnStMan but with per-cell hypercube
   management, adding complexity for the cell-to-hypercube mapping.

5. **TiledShapeStMan**: Multi-hypercube manager that creates separate
   hypercubes for each distinct cell shape. Significantly more complex than
   single-hypercube tiled managers.

6. **TiledDataStMan**: User-managed hypercubes with explicit addHypercube API.
   The most flexible tiled manager, requiring application-level hypercube
   lifecycle management.

### Corpus evidence

The local corpus includes:
- **pagedimage_coords**: Uses TiledCellStMan (table_keywords.bin only, no
  table.dat available for bucket-level testing)
- External artifacts in the corpus manifest reference all four tiled types, but
  these are casacore build tree artifacts and not shipped with casacore-mini

Cell-level data verification is available through the casacore interop tool,
which can open tables using casacore's native Table/Column APIs.

### Risk assessment

**Interoperability risk: LOW for current scope.**

The structural interoperability layer ensures casacore-mini can:
- Correctly identify which SM type manages each column
- Read and preserve all table.dat metadata through roundtrip
- Catalog SM data files for downstream processing

Cell-level data access would only be needed if casacore-mini must read or write
actual column values without casacore being available. For the current project
scope (metadata extraction and structural analysis), directory-level interop is
sufficient.

**Interoperability risk if cell-level access is later needed: MEDIUM.**

The bucket formats are complex but deterministic. Implementing read-only access
for SSM scalar columns would be the lowest-risk starting point, followed by SSM
array columns, then tiled managers.

## Carry-forward plan

If cell-level SM data access is required in a future phase:

1. **Phase 8 candidate: SSM read-only**
   - Implement `StandardStMan` bucket reader for scalar columns
   - Test against corpus fixtures and casacore-produced tables
   - Estimated scope: ~500 lines of new code + ~300 lines of tests

2. **Phase 8 candidate: SSM write**
   - Implement bucket writer for scalar columns
   - Validate roundtrip with casacore consumer
   - Estimated scope: ~400 lines additional

3. **Phase 9+ candidate: Tiled managers read-only**
   - Start with TiledColumnStMan (simplest tiled model)
   - Share hypercube/tiling infrastructure across all tiled managers
   - TiledShapeStMan and TiledDataStMan are the most complex and should be last

4. **ISM: Lowest priority**
   - IncrementalStMan is used primarily for MS subtables (FLAG_CATEGORY, etc.)
   - Delta-encoding model is complex but the usage pattern is narrow

## Decision record

- **Decision**: Defer all bucket-level SM implementations
- **Made by**: Autonomous agent per Phase 7 autonomy policy
- **Date**: 2026-02-24
- **Rationale**: Implementation complexity exceeds the wave scope; structural
  interoperability provides the required table-directory level coverage
- **Policy reference**: `docs/phase7/plan.md` section "deferral rules" item 1
  (corpus-backed evidence) and item 3 (do not block phase execution)
