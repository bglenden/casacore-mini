# P10-W2 Open Issues

- `std::submdspan` not available in current toolchain (C++26/P2630).
  Slice extraction uses manual iteration via `get_slice()`/`put_slice()`
  rather than submdspan views. Functionally equivalent; revisit if
  submdspan becomes available.
