#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12}"

cd "${ROOT_DIR}"

echo "=== P12-W2 wave gate ==="

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
check "tests" ctest --test-dir "${BUILD_DIR}" --output-on-failure -j8

# 3. Table::add_row API exists.
check "add_row-api" grep -q 'void add_row' include/casacore_mini/table.hpp

# 4. Table::remove_row API exists.
check "remove_row-api" grep -q 'void remove_row' include/casacore_mini/table.hpp

# 5. Dedicated mutation test exists and passes.
check "table_mutation_test" "${BUILD_DIR}/table_mutation_test"

echo ""
echo "=== P12-W2 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W2 gate FAILED" >&2
  exit 1
fi

echo "P12-W2 gate passed"
