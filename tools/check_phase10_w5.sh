#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W5 gate: region and mask persistence"

# 1. Verify lattice_region.hpp exists and declares key classes.
if [ ! -f "include/casacore_mini/lattice_region.hpp" ]; then
  echo "FAIL: include/casacore_mini/lattice_region.hpp not found" >&2
  exit 1
fi

for sym in "class LcRegion" "class LcBox" "class LcPixelSet" "class LcEllipsoid" \
           "class LcPolygon" "class LcMask" "class LcUnion" "class LcIntersection" \
           "class LcDifference" "class LcComplement" "class LcExtension" \
           "class LatticeRegion"; do
  if ! grep -q "${sym}" include/casacore_mini/lattice_region.hpp; then
    echo "FAIL: lattice_region.hpp missing '${sym}'" >&2
    exit 1
  fi
done

# 2. Verify image_region.hpp exists and declares WC region classes.
if [ ! -f "include/casacore_mini/image_region.hpp" ]; then
  echo "FAIL: include/casacore_mini/image_region.hpp not found" >&2
  exit 1
fi

for sym in "class WcRegion" "class WcBox" "class WcEllipsoid" "class WcPolygon" \
           "class WcUnion" "class WcIntersection" "class WcDifference" \
           "class WcComplement" "class ImageRegion" "class RegionHandler" \
           "class RegionManager"; do
  if ! grep -q "${sym}" include/casacore_mini/image_region.hpp; then
    echo "FAIL: image_region.hpp missing '${sym}'" >&2
    exit 1
  fi
done

# 3. Verify implementation files exist.
for src in "src/lattice_region.cpp" "src/image_region.cpp"; do
  if [ ! -f "${src}" ]; then
    echo "FAIL: ${src} not found" >&2
    exit 1
  fi
done

# 4. Build and run tests.
cmake -B "${BUILD_DIR}" -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF
cmake --build "${BUILD_DIR}" --target lattice_region_test image_region_test -j"$(nproc 2>/dev/null || sysctl -n hw.logicalcpu)"

echo "--- lattice_region_test ---"
"${BUILD_DIR}/lattice_region_test"

echo "--- image_region_test ---"
"${BUILD_DIR}/image_region_test"

# 5. Run full regression.
echo "--- full ctest ---"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "Phase 10 W5 gate: ALL PASSED"
