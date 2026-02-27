#!/usr/bin/env bash
# P10-W9 gate: utility-layer parity and integration cleanup
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p10}"

cd "${ROOT_DIR}"

echo "=== P10-W9 gate checks ==="

# 1. New classes/functions exist in headers
echo "[1] Checking new API surface in headers..."
grep -q "class LatticeConcat"  include/casacore_mini/lattice.hpp
grep -q "class RebinLattice"   include/casacore_mini/lattice.hpp
grep -q "class ImageExpr"      include/casacore_mini/image.hpp
grep -q "class ImageConcat"    include/casacore_mini/image.hpp
grep -q "struct ImageBeamSet"  include/casacore_mini/image.hpp
grep -q "struct MaskSpecifier" include/casacore_mini/image.hpp
grep -q "struct ImageSummary"  include/casacore_mini/image.hpp
grep -q "namespace image_utilities" include/casacore_mini/image.hpp
grep -q "image_regrid"         include/casacore_mini/image.hpp
grep -q "to_world"             include/casacore_mini/coordinate_system.hpp
grep -q "to_pixel"             include/casacore_mini/coordinate_system.hpp
echo "  All W9 API surface items present."

# 2. Dedicated test exists
echo "[2] Checking utility_parity_test registered..."
grep -q "utility_parity_test" CMakeLists.txt
echo "  Test registered."

# 3. Build
echo "[3] Building..."
cmake --build "$BUILD_DIR" 2>&1 | tail -3

# 4. Run utility_parity_test
echo "[4] Running utility_parity_test..."
"./$BUILD_DIR/utility_parity_test"

# 5. Full regression
echo "[5] Running full regression..."
ctest --test-dir "$BUILD_DIR" --output-on-failure 2>&1 | tail -5

echo "=== P10-W9 gate PASSED ==="
