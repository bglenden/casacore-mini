#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W3 gate: lattice core model and traversal"

# 1. Verify lattice.hpp exists and declares key classes.
if [ ! -f "include/casacore_mini/lattice.hpp" ]; then
  echo "FAIL: include/casacore_mini/lattice.hpp not found" >&2
  exit 1
fi

for sym in "class Lattice" "class MaskedLattice" "class ArrayLattice" "class PagedArray" "class SubLattice" "class TempLattice" "class LatticeIterator"; do
  if ! grep -q "${sym}" include/casacore_mini/lattice.hpp; then
    echo "FAIL: lattice.hpp missing '${sym}'" >&2
    exit 1
  fi
done

# 2. Verify lattice.cpp exists.
if [ ! -f "src/lattice.cpp" ]; then
  echo "FAIL: src/lattice.cpp not found" >&2
  exit 1
fi

# 3. Build and run test.
cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  > /dev/null 2>&1

cmake --build "${BUILD_DIR}" --target lattice_test -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" > /dev/null 2>&1

"${BUILD_DIR}/lattice_test"

echo "Phase 10 W3 gate passed"
