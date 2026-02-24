#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"
GENERATOR="${GENERATOR:-Ninja}"
CXX_BIN="${CXX:-clang++}"

cd "${ROOT_DIR}"

# Always start from a clean tree to avoid stale object/coverage artifacts.
rm -rf "${BUILD_DIR}"

bash tools/check_phase0.sh "${BUILD_DIR}"

cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER="${CXX_BIN}" \
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
bash tools/check_phase7_w3.sh "${BUILD_DIR}"
bash tools/check_phase7_w4.sh "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure
bash tools/check_coverage.sh "${BUILD_DIR}" 70

echo "build-lint-test-coverage checks passed"
