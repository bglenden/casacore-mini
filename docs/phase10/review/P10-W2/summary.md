# P10-W2 Summary: Lattice storage/view substrate

## Deliverables

1. `include/casacore_mini/lattice_shape.hpp` — `IPosition` (shape/index
   vector), `Slicer` (start/length/stride), `LatticeSpan<T,N>` /
   `ConstLatticeSpan<T,N>` mdspan aliases with `layout_left` (Fortran order),
   free functions for stride computation, linearization, delinearization,
   index/slicer validation.

2. `include/casacore_mini/lattice_array.hpp` — `LatticeArray<T>` owning
   multidimensional array with shared_ptr-based copy-on-write storage.
   Provides `const_view<N>()` / `view<N>()` returning `std::mdspan` views,
   element access (`at`, `put`), slice extraction (`get_slice`), and
   slice writing (`put_slice`).

3. `src/lattice_shape.cpp` — Implementations for IPosition, Slicer, and
   free functions.

4. `tests/lattice_shape_test.cpp` — 90 test assertions covering IPosition
   basics, equality, unsigned roundtrip, Slicer construction/validation,
   stride/index computation, LatticeArray construction, shared storage,
   mdspan 2D/3D views, contiguous and strided slicing.

## Infrastructure changes

- C++ standard bumped from C++20 to C++23 (required for `std::mdspan`).
- Added `#include <functional>` to `ms_writer.hpp` (C++23 stricter implicit
  include resolution).

## Gate result

`bash tools/check_phase10_w2.sh build-p10` — PASSED (90/90 tests)
Full regression: 68/68 pre-existing tests still pass.
