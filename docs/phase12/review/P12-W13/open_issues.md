# P12-W13 Open Issues

1. **vector<bool> fallback**: `strided_fortran_copy` and `strided_fortran_scatter`
   cannot be used with `vector<bool>` due to its proxy reference semantics and
   missing `.data()`. The bool specialization retains element-wise access.
   This is a known limitation of `vector<bool>` in C++ and not specific to our
   implementation.

2. **Additional migration candidates**: `LatticeIterator<T>` position tracking
   and `RebinLattice<T>` bin iteration could benefit from strided helpers but
   are lower priority since they work correctly as-is. Can be migrated in a
   future pass if needed.
