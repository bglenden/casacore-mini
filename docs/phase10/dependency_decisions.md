# Phase 10 Dependency Decisions

Locked at: 2026-02-26 (P10-W1)

## Multidimensional storage substrate

**Decision:** Use `std::mdspan` (C++23) with `layout_left` (Fortran/column-major
order, matching casacore's Array convention). Fall back to the reference
implementation (`Kokkos/mdspan`) if the compiler's standard library does not
provide `<mdspan>`.

**Rationale:** `std::mdspan` is standardized, zero-overhead, and provides the
view/slice semantics needed for lattice traversal without a bespoke container.
Column-major layout matches casacore's Array storage order.

## Slice/sub-view adapter

**Decision:** Use `std::submdspan` (C++26 / P2630) from the Kokkos reference
implementation if not available natively.

**Rationale:** Required for SubLattice views and region-based subsetting.

## Expression parser

**Decision:** Hand-written recursive-descent parser for LEL expressions.
No external parser-generator dependency (no flex/bison/ANTLR).

**Rationale:** The LEL grammar is small enough for a hand-written parser.
Avoids build-time code generation complexity and keeps the dependency
footprint minimal. Casacore's own LEL parser is hand-written.

## Expression evaluator

**Decision:** Tree-walk evaluator over the parsed AST. No JIT, no bytecode.

**Rationale:** LEL expressions are evaluated once over full lattice tiles.
The bottleneck is I/O and memory bandwidth, not expression dispatch overhead.
A simple tree-walk evaluator matches casacore's own approach.

## Image table persistence

**Decision:** Reuse existing `Table` and storage manager infrastructure
from Phases 4-7. PagedImage stores pixel data as a TiledShapeStMan column
in a casacore-format table.

**Rationale:** casacore's PagedImage is backed by a Table with a
TiledCellStMan/TiledShapeStMan column. Reusing the existing table layer
provides automatic interoperability.

## FITS/WCS

**Decision:** Continue using `cfitsio` and `wcslib` (already linked from Phase 8).

**Rationale:** No new FITS requirements. Coordinate system integration
already functional.

## No new external dependencies

No additional third-party libraries are introduced in Phase 10 beyond what
is already linked (ERFA, WCSLIB, cfitsio, Kokkos/mdspan reference impl).
