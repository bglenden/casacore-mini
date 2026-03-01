#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12}"

cd "${ROOT_DIR}"

echo "=== P12-W5 wave gate ==="

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
check "taql_join_test" "${BUILD_DIR}/taql_join_test"

# Verify multi-table overload exists.
check "multi-table-overload" \
  sh -c 'grep -q "unordered_map<std::string, Table\*>" src/taql.cpp'

# Verify JOIN execution logic exists.
check "join-execution" \
  sh -c 'grep -q "join_on" src/taql.cpp'

# Verify resolve_column with alias.dot support.
check "alias-column-resolve" \
  sh -c 'grep -q "resolve_column" src/taql.cpp'

echo ""
echo "=== P12-W5 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W5 gate FAILED" >&2
  exit 1
fi

echo "P12-W5 gate passed"
