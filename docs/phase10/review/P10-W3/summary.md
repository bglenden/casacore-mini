# P10-W3 Summary: Lattice core model and traversal

## Deliverables

1. `include/casacore_mini/lattice.hpp` — Full lattice class hierarchy:
   - `Lattice<T>` — abstract base with get/put/set/apply interface
   - `MaskedLattice<T>` — abstract base adding mask support
   - `ArrayLattice<T>` — in-memory lattice backed by LatticeArray
   - `PagedArray<T>` — disk-backed lattice via Table + TiledShapeStMan
   - `SubLattice<T>` — view into a sub-region of a parent lattice
   - `TempLattice<T>` — auto memory/disk hybrid
   - `LatticeIterator<T>` — cursor-based tile iteration

2. `src/lattice.cpp` — PagedArray explicit instantiations for float,
   double, complex<float> with table-backed persistence.

3. `tests/lattice_test.cpp` — 58 test assertions covering:
   - ArrayLattice construction, read/write, slicing, set, apply, mask
   - SubLattice read, write, read-only enforcement
   - TempLattice memory-mode operation
   - LatticeIterator basic traversal, edge chunks, write-back, reset

## Key design points

- All template code in header (lattice.hpp) except PagedArray I/O
  specializations which live in lattice.cpp.
- SubLattice maps local coordinates to parent coordinates via
  to_parent_index() / to_parent_slicer().
- LatticeIterator handles edge chunks (smaller than cursor at boundaries).
- LatticeArray::at() and operator[] return by value (not reference) to
  support vector<bool>.

## Gate result

`bash tools/check_phase10_w3.sh build-p10` — PASSED (58/58 tests)
Full regression: 69/69 tests pass.
