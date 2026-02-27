# P10-W9 Open Issues

## 1. LatticeConcat::get_slice() reads entire lattice
Current implementation does `get()` then slices in memory. For large
lattices this is inefficient. Could be optimized to read only the
relevant sub-ranges from component lattices.

## 2. RebinLattice iterates all output elements
The nested-loop rebinning works for any dimensionality but has O(N*M)
complexity where N is output elements and M is bin size. For large
lattices, a tiled approach would be more cache-friendly.

## 3. image_regrid is nearest-neighbor only
No interpolation options (bilinear, cubic). Sufficient for API parity
testing but would need enhancement for production use.
