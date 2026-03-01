#!/usr/bin/env bash
set -euo pipefail

# Phase 12, Wave 1: Coordinate regression closure + diagnostics hardening.
# Requires casacore development package.

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12-w1}"

cd "${ROOT_DIR}"

echo "=== P12-W1 wave gate ==="

PASS=0
FAIL=0

check() {
  local label="$1"
  shift
  if "$@" >/dev/null 2>&1; then
    echo "  PASS: ${label}"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: ${label}" >&2
    FAIL=$((FAIL + 1))
  fi
}

# 1. Build passes.
cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF >/dev/null 2>&1

check "build" cmake --build "${BUILD_DIR}" -j8

# 2. All tests pass.
check "tests" cmake --build "${BUILD_DIR}" --target test

# 3. SpectralCoordinate saves crval at top level (not nested in wcs).
check "spectral-save-toplevel-crval" \
  grep -q 'rec.set("crval"' src/spectral_coordinate.cpp

# 4. Phase 8 matrix runs (if casacore available).
if pkg-config --exists casacore 2>/dev/null; then
  echo "  casacore available, running Phase 8 matrix..."
  if bash tools/interop/run_phase8_matrix.sh "${BUILD_DIR}" >/dev/null 2>&1; then
    echo "  PASS: phase8-matrix-24/24"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: phase8-matrix" >&2
    FAIL=$((FAIL + 1))
  fi
else
  echo "  SKIP: casacore not available for matrix check"
fi

# 5. Matrix runner emits per-cell stderr logs (not /dev/null).
check "matrix-stderr-diagnostics" \
  grep -q 'cell_log=' tools/interop/run_phase8_matrix.sh

echo ""
echo "=== P12-W1 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W1 gate FAILED" >&2
  exit 1
fi

echo "P12-W1 gate passed"
