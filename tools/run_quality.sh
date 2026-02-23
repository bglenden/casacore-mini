#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"
GENERATOR="${GENERATOR:-Ninja}"

cd "${ROOT_DIR}"

bash tools/check_format.sh
bash tools/check_phase0.sh "${BUILD_DIR}"

cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=ON \
  -DCASACORE_MINI_ENABLE_COVERAGE=ON \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF

cmake --build "${BUILD_DIR}"
bash tools/check_phase1.sh "${BUILD_DIR}"
bash tools/check_phase2.sh

# Reset old runtime counters so repeated local runs produce deterministic coverage.
find "${BUILD_DIR}" -type f -name '*.gcda' -delete 2>/dev/null || true

ctest --test-dir "${BUILD_DIR}" --output-on-failure
bash tools/check_coverage.sh "${BUILD_DIR}" 70

echo "All quality checks passed"
