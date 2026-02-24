#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"
DOC_BUILD_DIR="${2:-${BUILD_DIR}-doc}"
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
bash tools/check_phase2.sh "${BUILD_DIR}"
bash tools/check_phase3.sh
bash tools/check_phase4.sh
bash tools/check_phase5.sh "${BUILD_DIR}"
bash tools/check_phase6.sh "${BUILD_DIR}"

cmake -S . -B "${DOC_BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=ON
cmake --build "${DOC_BUILD_DIR}" --target doc

# Reset old runtime counters so repeated local runs produce deterministic coverage.
find "${BUILD_DIR}" -type f -name '*.gcda' -delete 2>/dev/null || true

ctest --test-dir "${BUILD_DIR}" --output-on-failure
bash tools/check_coverage.sh "${BUILD_DIR}" 70

echo "All quality checks passed"
