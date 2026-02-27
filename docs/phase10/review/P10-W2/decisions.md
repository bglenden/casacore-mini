# P10-W2 Decisions

## D1: C++ standard bump to C++23

**Decision:** Bump from C++20 to C++23.

**Rationale:** `std::mdspan` requires C++23 standard library support.
Compiler (AppleClang 17) and libc++ fully support this. All 68
pre-existing tests pass without changes (one missing `<functional>`
include fixed in ms_writer.hpp).

## D2: LatticeArray copy-on-write via shared_ptr

**Decision:** `LatticeArray<T>` uses `std::shared_ptr<std::vector<T>>`
for storage. Copies share storage; `make_unique()` triggers deep copy.

**Rationale:** Matches casacore's COWPtr pattern. Avoids expensive
copies during expression evaluation and iterator operations. Users
call `make_unique()` before mutation.

## D3: Slice operations as copy, not view

**Decision:** `get_slice()` returns a new `LatticeArray` (copy), not
a sub-view. `put_slice()` writes into the target array.

**Rationale:** `std::submdspan` is not available. Copy semantics are
simpler and match the casacore `getSlice()`/`putSlice()` API pattern
where slices are materialized into caller-owned buffers.
