# P12-W13: mdspan foundation + migration

## Scope
Extend mdspan foundation with stride-based copy/scatter helpers, migrate
LatticeArray internal slicing to use mdspan-style stride arithmetic, and add
convenience functions for creating mdspan views from flat data + shape.

## Implementation

### Foundation: mdspan_compat.hpp (pre-existing)
Already provided three-tier fallback: native `<mdspan>`, experimental, and
local shim with layout_left (Fortran order) support. No changes needed.

### New helpers in lattice_shape.hpp

1. **`strided_fortran_copy()`**: Copies elements from a strided source array
   to a dense destination array using Fortran-order stride arithmetic.
   Replaces manual IPosition-based index computation with direct stride
   offset tracking — the core of mdspan's layout_left mapping.

2. **`strided_fortran_scatter()`**: Inverse of strided_copy — scatters
   elements from a dense source into strided destination positions.

3. **`make_const_lattice_span<T, N>()`**: Creates a `ConstLatticeSpan<T, N>`
   (mdspan view) from a raw pointer and IPosition shape. Useful at
   table-array boundaries where data arrives as flat vectors.

4. **`make_lattice_span<T, N>()`**: Mutable version.

### LatticeArray migration

Refactored `get_slice()` to use `strided_fortran_copy()` and `put_slice()`
to use `strided_fortran_scatter()`. The `vector<bool>` specialization
maintains element-wise fallback (no `.data()` pointer available).

### API preservation
- External API unchanged: all methods return same types
- Fortran (column-major) ordering strictly preserved
- No on-disk format changes
- All 108 existing tests pass

## Test coverage
- `mdspan_migration_test.cpp`: 50 checks covering:
  - Unit-stride and strided copy
  - Scatter to strided destination
  - 2D and 3D strided copy
  - LatticeArray get_slice/put_slice parity
  - make_const_lattice_span / make_lattice_span
  - mdspan compat shim basic functionality

## Wave gate
10/10 checks pass.
