# Phase 10 Interop Artifact Inventory

## Required artifacts (5)

Each artifact must pass all 4 matrix cells:
- casacore -> casacore
- casacore -> casacore-mini
- casacore-mini -> casacore
- casacore-mini -> casacore-mini

Total: 5 artifacts x 4 directions = 20 matrix cells.

---

## Artifact 1: img-minimal

Minimal valid PagedImage<Float> with 2D shape (64x64), default coordinate
system (linear axes), no mask, no regions.

### Producer output
- Root directory with `table.dat`, `table.info`, `table.lock`
- Single data column (`map`) with Float pixel values
- Coordinate system keywords (direction, spectral, or linear as applicable)

### Semantic checks
1. `table.info` type == "Image"
2. Pixel shape == (64, 64)
3. All pixel values round-trip within float tolerance
4. Coordinate system keywords present and consistent
5. No mask column present

---

## Artifact 2: img-cube-masked

3D image cube (32x32x16) of Float with attached pixel mask.

### Producer output
- PagedImage<Float> with shape (32, 32, 16)
- Coordinate system with direction + spectral axes
- Pixel mask with ~25% of voxels masked (deterministic pattern)
- Pixel values set to known function of coordinates

### Semantic checks
1. Shape == (32, 32, 16)
2. Pixel values match within float tolerance
3. Mask values match exactly (boolean)
4. Coordinate system axes, reference values, increments match
5. Image info (units, restoring beam if set) matches

---

## Artifact 3: img-region-subset

Image with named region producing a SubImage view.

### Producer output
- Base PagedImage<Float> with shape (64, 64)
- Named LCBox region stored in image keywords
- SubImage extracted using the stored region

### Semantic checks
1. Base image pixel values round-trip
2. Named region persists and can be retrieved
3. SubImage shape matches region bounding box
4. SubImage pixel values match corresponding base image pixels
5. Region type and parameters round-trip correctly

---

## Artifact 4: img-expression

Image produced by evaluating a LEL expression over lattice inputs.

### Producer output
- Two input ArrayLattice<Float> with shape (32, 32)
- Expression: arithmetic combination (e.g., `lat1 + 2.0 * lat2`)
- Result stored as PagedImage

### Semantic checks
1. Output shape matches input shape
2. Output pixel values match expected arithmetic result within float tolerance
3. Expression includes at least: addition, scalar multiplication, lattice operand
4. If mask-aware: masked pixels propagate correctly

---

## Artifact 5: img-complex

PagedImage<Complex> with complex float pixel data.

### Producer output
- PagedImage<Complex> with shape (32, 32)
- Complex pixel values with non-trivial real and imaginary parts
- Coordinate system with linear axes

### Semantic checks
1. Shape == (32, 32)
2. Real and imaginary parts round-trip within float tolerance (independently)
3. Coordinate system keywords present and consistent
4. Data type reported as Complex
