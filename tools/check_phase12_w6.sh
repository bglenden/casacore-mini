#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12}"

cd "${ROOT_DIR}"

echo "=== P12-W6 wave gate ==="

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
check "taql_aggregate_test" "${BUILD_DIR}/taql_aggregate_test"

# Verify GROUPBY no longer throws "not supported".
check "groupby-implemented" \
  sh -c '! grep -q "GROUPBY is not yet supported" src/taql.cpp'

# Verify aggregate evaluation exists.
check "eval_aggregate-exists" \
  sh -c 'grep -q "eval_aggregate" src/taql.cpp'

# Verify key aggregate functions are handled.
check "gsum-handler" sh -c 'grep -q "GSUM" src/taql.cpp'
check "gmin-handler" sh -c 'grep -q "GMIN" src/taql.cpp'
check "gmean-handler" sh -c 'grep -q "GMEAN" src/taql.cpp'

echo ""
echo "=== P12-W6 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W6 gate FAILED" >&2
  exit 1
fi

echo "P12-W6 gate passed"
