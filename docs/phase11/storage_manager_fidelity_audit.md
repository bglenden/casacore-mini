# Phase 11 Storage Manager Fidelity Audit

Date: 2026-02-27

## Purpose

Audit all storage-manager reader+writer paths for behavioral simplifications
or heuristics compared to upstream casacore. Classify each finding as `Exact`
or `Simplified` with remediation plan.

## Methodology

1. Reviewed all SM reader/writer source in `src/` against upstream equivalents
   in `casacore-original/tables/DataMan/`
2. Cross-referenced with Phase 7 interop matrix results (all 24/24 cells pass)
3. Cross-referenced with Phase 10 interop matrix results (all 20/20 cells pass)

## Findings by Storage Manager

### StandardStMan (SSM)

| Component | Status | Notes |
|-----------|--------|-------|
| SSM reader - bucket file parsing | Exact | Full bucket cache, index, and data parsing |
| SSM reader - all scalar types | Exact | All DataType values supported |
| SSM reader - fixed-shape arrays | Exact | Direct bucket reads |
| SSM reader - indirect (variable) arrays | Exact | Offset/shape header parsing |
| SSM reader - string columns | Exact | Heap-based string storage |
| SSM reader - endian handling | Exact | Both big/little endian supported |
| SSM writer - bucket management | Exact | Bucket allocation and flushing |
| SSM writer - all scalar types | Exact | All DataType values written |
| SSM writer - fixed-shape arrays | Exact | Direct bucket writes |
| SSM writer - indirect arrays | Exact | Offset/shape header writes |

### IncrementalStMan (ISM)

| Component | Status | Notes |
|-----------|--------|-------|
| ISM reader - bucket/index parsing | Exact | Bucketed index with last-known-value semantics |
| ISM reader - all scalar types | Exact | Full type matrix |
| ISM reader - string columns | Exact | String heap reads |
| ISM reader - endian handling | Exact | Both endians supported |
| ISM writer - not integrated into Table::create | Simplified | Writer exists but not wired to create API |

**Remediation**: P11-W4 will integrate ISM writer into `Table::create()` via
`TableCreateOptions::sm_type = "IncrementalStMan"`.

### TiledColumnStMan

| Component | Status | Notes |
|-----------|--------|-------|
| Reader - tile/hypercube parsing | Exact | Full tile cache and hypercube navigation |
| Reader - all numeric types | Exact | Including Bool bit-packing (Phase 10 fix) |
| Reader - endian handling | Exact | Both endians supported |
| Writer - tile allocation | Exact | Full tile writes |
| Writer - not integrated into Table::create | Simplified | Writer exists but create API doesn't expose |

**Remediation**: P11-W4.

### TiledCellStMan

| Component | Status | Notes |
|-----------|--------|-------|
| Reader - per-row hypercube | Exact | Cell-specific tile navigation |
| Reader - all numeric types | Exact | Including Bool bit-packing |
| Writer - per-row tile allocation | Exact | Cell-specific writes |
| Writer - not integrated into Table::create | Simplified | Writer exists but create API doesn't expose |

**Remediation**: P11-W4.

### TiledShapeStMan

| Component | Status | Notes |
|-----------|--------|-------|
| Reader - shape-grouped hypercubes | Exact | Shape-based tile grouping |
| Reader - all numeric types | Exact | Including Bool bit-packing |
| Writer - shape grouping | Exact | Correct shape-to-hypercube mapping |
| Writer - not integrated into Table::create | Simplified | Writer exists but create API doesn't expose |

**Remediation**: P11-W4.

### TiledDataStMan

| Component | Status | Notes |
|-----------|--------|-------|
| Reader - coordinate-based hypercubes | Exact | Full coordinate tile navigation |
| Reader - all numeric types | Exact | Including Bool bit-packing |
| Writer - coordinate tile allocation | Exact | Correct coordinate-to-hypercube mapping |
| Writer - not integrated into Table::create | Simplified | Writer exists but create API doesn't expose |

**Remediation**: P11-W4.

## Summary

| Storage Manager | Reader | Writer | Create Integration |
|----------------|--------|--------|-------------------|
| StandardStMan | Exact | Exact | Exact (default) |
| IncrementalStMan | Exact | Exact | Simplified -> P11-W4 |
| TiledColumnStMan | Exact | Exact | Simplified -> P11-W4 |
| TiledCellStMan | Exact | Exact | Simplified -> P11-W4 |
| TiledShapeStMan | Exact | Exact | Simplified -> P11-W4 |
| TiledDataStMan | Exact | Exact | Simplified -> P11-W4 |

All reader and writer implementations are fidelity-exact per Phase 7 and
Phase 10 interop matrix evidence. The only simplification is that ISM and
TSM writers are not yet routed through `Table::create()`, which will be
resolved in P11-W4.
