#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-fast-local}"
GENERATOR="${GENERATOR:-Ninja}"
CXX_BIN="${CXX:-clang++}"

cd "${ROOT_DIR}"

# Keep fast CI deterministic and avoid stale build artifacts.
rm -rf "${BUILD_DIR}"

# Cheap manifest/determinism gate retained in fast path.
bash tools/check_phase0.sh "${BUILD_DIR}"

cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER="${CXX_BIN}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF

cmake --build "${BUILD_DIR}"
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "build-test checks passed"
