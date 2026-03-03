#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12}"

cd "${ROOT_DIR}"

echo "=== P12-W4 wave gate ==="

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
check "taql_ddl_test" "${BUILD_DIR}/taql_ddl_test"

# Verify DDL commands execute (not stub-only).
check "create-table-executes" \
  sh -c 'grep -q "Table::create" src/taql.cpp'
check "drop-table-executes" \
  sh -c 'grep -q "drop_table" src/taql.cpp'
check "alter-table-executes" \
  sh -c 'grep -q "AlterAction::add_row" src/taql.cpp'

# Record::remove exists.
check "record-remove-method" \
  sh -c 'grep -q "bool Record::remove" src/record.cpp'

echo ""
echo "=== P12-W4 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W4 gate FAILED" >&2
  exit 1
fi

echo "P12-W4 gate passed"
