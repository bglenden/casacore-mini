# P7-W10 Decisions

## Decision 1: Phase 7 complete at structural interop level
**Choice**: Close Phase 7 with structural (directory-level) interop for all six
storage managers, deferring cell-level data access to Phase 8.

**Rationale**: All wave gates pass. The interop matrix covers both producers and
both consumers for all six SM types at the metadata level. Cell-level access
requires reverse-engineering undocumented bucket formats, which is a distinct
scope suitable for a dedicated phase.

## Decision 2: Phase 8 recommendation prioritizes SSM scalar read
**Choice**: Recommend starting Phase 8 with StandardStMan scalar read-only as
the lowest-complexity entry point into bucket-level access.

**Rationale**: SSM is the default and most common storage manager. Scalar
columns are simpler than array columns. Read-only avoids the complexity of
bucket allocation and free-list management. This path provides the most value
for the least implementation risk.
