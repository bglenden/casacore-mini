#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12}"

cd "${ROOT_DIR}"

echo "=== P12-W3 wave gate ==="

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
check "taql_dml_test" "${BUILD_DIR}/taql_dml_test"

# Verify INSERT/DELETE no longer throw "not supported".
check "insert-implemented" \
  sh -c '! grep -q "INSERT is not supported" src/taql.cpp'
check "delete-implemented" \
  sh -c '! grep -q "DELETE row removal is not supported" src/taql.cpp'

echo ""
echo "=== P12-W3 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W3 gate FAILED" >&2
  exit 1
fi

echo "P12-W3 gate passed"
