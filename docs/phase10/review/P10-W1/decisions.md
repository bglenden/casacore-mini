# P10-W1 Decisions

## D1: Multidimensional storage model

**Decision:** `std::mdspan` with `layout_left` (Fortran order), reference
implementation fallback from Kokkos.

**Rationale:** Matches casacore's column-major Array storage. Standardized,
zero-overhead view semantics.

## D2: Expression parser approach

**Decision:** Hand-written recursive-descent parser. No external parser
generators.

**Rationale:** LEL grammar is small. Avoids build-time codegen. Matches
casacore's own approach.

## D3: Expression evaluator approach

**Decision:** Tree-walk evaluator over AST nodes. No JIT or bytecode.

**Rationale:** I/O-bound workloads. Expression dispatch overhead is negligible.
Simple and matches upstream.

## D4: Image persistence backend

**Decision:** Reuse Phase 4-7 Table + TiledShapeStMan infrastructure.

**Rationale:** casacore's PagedImage is Table-backed. Reusing the existing
table layer gives automatic interoperability.

## D5: Lint profile

**Decision:** Inherit Phase 9 lint profile unchanged.

**Rationale:** No new check categories needed. Phase 9 profile is stable
and well-tested.

## D6: No new external dependencies

No new third-party libraries introduced. Continue with ERFA, WCSLIB, cfitsio,
and Kokkos/mdspan reference implementation.
