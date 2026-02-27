# P10-W6 Open Issues

- LEL string parsing is deferred to W7. Currently expressions are built
  programmatically via builder helpers (lel_add, lel_scalar, etc.).
- Complex type support (complex64, complex128) is declared in LelType but
  no AST nodes are instantiated for complex types yet. Deferred to W7.
- LelArrayRef currently takes a raw vector + optional mask. Integration
  with LatticeArray (reading tiles from disk) is a W8 task.
- Median reduction uses full sort (O(n log n)). Could use nth_element
  for O(n) average case if performance matters.
- No chunk/tile-based evaluation yet. Current eval() materializes the
  entire result array. Tiled evaluation deferred to W8.
