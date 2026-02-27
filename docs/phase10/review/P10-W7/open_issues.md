# P10-W7 Open Issues

- Parser does not yet support complex literals (e.g., "1.0+2.0i"). Complex
  lattices must be created programmatically via formComplex or LelArrayRef.
- Parser function dispatch for complex-typed lattice arguments (real(), imag(),
  conj(), arg(), abs()) is not yet wired through the string parser. These
  must be built programmatically. Full parser integration deferred to W11.
- Parser does not support operator overloading syntax (e.g., `lat1 + lat2`
  where types differ). Mixed-type operations work only between float and
  double, not between real and complex.
- String-based expression round-trip (serialize AST back to string) is not
  implemented. Deferred as non-required.
- The parser treats all numeric literals as float. A suffix syntax (e.g.,
  "1.0d" for double) could be added if needed.
