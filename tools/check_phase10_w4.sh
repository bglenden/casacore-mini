#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W4 gate: image core models and metadata lifecycle"

# 1. Verify image.hpp exists and declares key classes.
if [ ! -f "include/casacore_mini/image.hpp" ]; then
  echo "FAIL: include/casacore_mini/image.hpp not found" >&2
  exit 1
fi

for sym in "struct ImageInfo" "class ImageInterface" "class PagedImage" "class TempImage" "class SubImage"; do
  if ! grep -q "${sym}" include/casacore_mini/image.hpp; then
    echo "FAIL: image.hpp missing '${sym}'" >&2
    exit 1
  fi
done

# 2. Verify image.cpp exists.
if [ ! -f "src/image.cpp" ]; then
  echo "FAIL: src/image.cpp not found" >&2
  exit 1
fi

# 3. Build and run test.
cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=OFF \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  > /dev/null 2>&1

cmake --build "${BUILD_DIR}" --target image_test -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" > /dev/null 2>&1

"${BUILD_DIR}/image_test"

echo "Phase 10 W4 gate passed"
