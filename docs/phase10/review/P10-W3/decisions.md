# P10-W3 Decisions

## D1: Template code in header

**Decision:** All lattice template implementations live in lattice.hpp.
Only PagedArray type-specific I/O specializations are in lattice.cpp.

**Rationale:** Standard C++ template instantiation model. Avoids explicit
instantiation for every user type combination except where non-template
Table API calls require it.

## D2: LatticeArray return-by-value

**Decision:** `LatticeArray::at()` and `operator[]` return `T` by value
instead of `const T&`.

**Rationale:** `std::vector<bool>` uses proxy references incompatible
with returning `const T&`. By-value return works for all types and is
consistent with casacore's value semantics.

## D3: SubLattice coordinate mapping

**Decision:** SubLattice stores a reference to the parent and a Slicer,
translating all local operations to parent coordinates.

**Rationale:** Avoids data duplication. Matches casacore SubLattice
semantics where the sub-lattice is a live view into the parent.
