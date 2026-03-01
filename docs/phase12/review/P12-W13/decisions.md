# P12-W13 Decisions

1. **Stride arithmetic over submdspan**: Rather than introducing `std::submdspan`
   (which requires additional compatibility shim complexity), implemented
   `strided_fortran_copy` and `strided_fortran_scatter` which directly compute
   offsets using stride vectors. This achieves the same result with simpler code
   and no additional compatibility concerns.

2. **Helpers in lattice_shape.hpp not separate file**: The strided helpers are
   closely related to existing Fortran-order utilities (fortran_strides,
   linear_index) and share the same header. Creating a separate
   `mdspan_helpers.hpp` would fragment related utilities.

3. **No table API changes**: Array columns continue to use `std::vector<T>` at
   the API boundary. The `make_*_lattice_span` helpers allow callers to create
   mdspan views over these vectors when multidimensional access is needed,
   without changing the storage or serialization model.

4. **No mdspan_compat.hpp changes**: The existing three-tier fallback is
   sufficient. The shim supports all required features (dextents, layout_left,
   operator[]) and is already tested.
