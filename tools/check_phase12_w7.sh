#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12}"

cd "${ROOT_DIR}"

echo "=== P12-W7 wave gate ==="

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

cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF >/dev/null 2>&1

check "build" cmake --build "${BUILD_DIR}" -j8
check "tests" ctest --test-dir "${BUILD_DIR}" --output-on-failure -j8
check "taql_udf_equiv_test" "${BUILD_DIR}/taql_udf_equiv_test"

# Verify datetime functions exist.
check "datetime-functions" \
  sh -c 'grep -q "YEAR\|MONTH\|DAY\|CMONTH" src/taql.cpp'

# Verify angle functions exist.
check "angle-functions" \
  sh -c 'grep -q "HMS\|DMS\|NORMANGLE\|ANGDIST" src/taql.cpp'

# Verify array functions exist.
check "array-functions" \
  sh -c 'grep -q "ARRSUM\|ARRMIN\|ARRMAX\|ARRMEAN" src/taql.cpp'

# Verify complex functions exist.
check "complex-functions" \
  sh -c 'grep -q "CONJ\|COMPLEX" src/taql.cpp'

# Verify unit conversion exists.
check "unit-conversion" \
  sh -c 'grep -q "unit_to_si" src/taql.cpp'

# Verify measure conversion bridge exists.
check "meas-udf-bridge" \
  sh -c 'grep -q "MEAS_DIR_\|MEAS_EPOCH_\|MEAS_POS_" src/taql.cpp'

echo ""
echo "=== P12-W7 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W7 gate FAILED" >&2
  exit 1
fi

echo "P12-W7 gate passed"
