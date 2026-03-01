# Deferred-on-Demand Storage Manager Policy

Date: 2026-02-28
Status: Active

## Core manager set (in scope)

The following six storage managers are fully implemented with fidelity-exact
behavior for all operations (create, open, read, write, add_row, remove_row):

1. **StandardStMan** — bucket-based scalar/array storage (default)
2. **IncrementalStMan** — last-known-value chained scalar storage
3. **TiledColumnStMan** — column-oriented tiled array storage
4. **TiledCellStMan** — per-row tiled array storage
5. **TiledShapeStMan** — shape-grouped tiled array storage
6. **TiledDataStMan** — coordinate-based tiled array storage

## Deferred managers (DeferredOnDemand)

| Manager | Upstream location | Status | Trigger to promote |
|---------|-------------------|--------|-------------------|
| MemoryStMan | `tables/DataMan/MemoryStMan` | DeferredOnDemand | A concrete user workflow requires in-memory-only table storage with no disk persistence |
| Adios2StMan | `tables/DataMan/Adios2StMan` | DeferredOnDemand | An ADIOS2-backed dataset artifact is encountered that cannot be converted to standard table format |
| Dysco | `tables/DataMan/Dysco` | DeferredOnDemand | A Dysco-compressed MS artifact is encountered that must be read without prior decompression |
| AlternateMans | various | DeferredOnDemand | A specific alternate-manager artifact is encountered in a required workflow |

## Demand-trigger procedure

To promote a deferred manager into scope:

1. **Evidence**: Provide a concrete artifact (file, dataset, or workflow description)
   that cannot be handled by the existing six managers.
2. **Impact**: Document which user operations are blocked without the manager.
3. **Scope assessment**: Estimate implementation effort (reader only vs. reader+writer,
   special features like compression/decompression).
4. **Plan update**: Add the manager to the active plan wave with implementation tasks.
5. **Approval**: Owner sign-off that the evidence justifies inclusion.

## Policy rules

1. Deferred managers do not block Phase 12 closeout.
2. If a deferred manager is promoted during Phase 12, it gets its own wave or is
   appended to an existing wave.
3. Reader-only implementations are acceptable for initial promotion if the manager
   is primarily needed for reading existing datasets.
4. Promoted managers must pass the same fidelity standard as core managers:
   exact behavior with interop matrix evidence where applicable.
