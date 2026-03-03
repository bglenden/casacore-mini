#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W2 gate: lattice storage/view substrate"

# 1. Verify required headers exist.
for hdr in lattice_shape.hpp lattice_array.hpp; do
  if [ ! -f "include/casacore_mini/${hdr}" ]; then
    echo "FAIL: include/casacore_mini/${hdr} not found" >&2
    exit 1
  fi
done

# 2. Verify source exists.
if [ ! -f "src/lattice_shape.cpp" ]; then
  echo "FAIL: src/lattice_shape.cpp not found" >&2
  exit 1
fi

# 3. Verify key types are declared.
for sym in "class IPosition" "class Slicer" "class LatticeArray" "LatticeSpan" "ConstLatticeSpan" "mdspan"; do
  if ! grep -rq "${sym}" include/casacore_mini/lattice_shape.hpp include/casacore_mini/lattice_array.hpp; then
    echo "FAIL: symbol '${sym}' not found in lattice headers" >&2
    exit 1
  fi
done

# 4. Verify Fortran-order layout_left is used.
if ! grep -q "layout_left" include/casacore_mini/lattice_shape.hpp; then
  echo "FAIL: layout_left not found in lattice_shape.hpp" >&2
  exit 1
fi

# 5. Build and run test.
cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  > /dev/null 2>&1

cmake --build "${BUILD_DIR}" --target lattice_shape_test -j"$(sysctl -n hw.ncpu 2>/dev/null || nproc)" > /dev/null 2>&1

"${BUILD_DIR}/lattice_shape_test"

echo "Phase 10 W2 gate passed"
