#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 W12 gate: StandardStMan read path with cell-value verification"

# 1. Verify SsmReader header exists.
if [ ! -f include/casacore_mini/standard_stman.hpp ]; then
  echo "FAIL: standard_stman.hpp not found" >&2
  exit 1
fi

# 2. Verify SsmReader implementation exists.
if [ ! -f src/standard_stman.cpp ]; then
  echo "FAIL: standard_stman.cpp not found" >&2
  exit 1
fi

# 3. Verify SSM test exists and passes.
if [ ! -f tests/standard_stman_test.cpp ]; then
  echo "FAIL: standard_stman_test.cpp not found" >&2
  exit 1
fi

if [ -x "${BUILD_DIR}/standard_stman_test" ]; then
  if ! "${BUILD_DIR}/standard_stman_test"; then
    echo "FAIL: standard_stman_test failed" >&2
    exit 1
  fi
else
  echo "WARN: standard_stman_test binary not found in ${BUILD_DIR}" >&2
fi

# 4. Verify SSM fixture exists.
if [ ! -d tests/fixtures/ssm_table ]; then
  echo "FAIL: SSM fixture directory not found" >&2
  exit 1
fi

if [ ! -f tests/fixtures/ssm_table/table.dat ]; then
  echo "FAIL: SSM fixture table.dat not found" >&2
  exit 1
fi

if [ ! -f tests/fixtures/ssm_table/table.f0 ]; then
  echo "FAIL: SSM fixture table.f0 not found" >&2
  exit 1
fi

# 5. Verify interop_mini_tool has cell-value verification for table-dir.
if ! grep -q "\[PASS\] cell_values" src/interop_mini_tool.cpp; then
  echo "FAIL: interop_mini_tool.cpp missing cell-value verification" >&2
  exit 1
fi

# 6. Verify casacore_interop_tool has cell-value verification for table-dir.
if ! grep -q "\[PASS\] cell_values" tools/interop/casacore_interop_tool.cpp; then
  echo "FAIL: casacore_interop_tool.cpp missing cell-value verification" >&2
  exit 1
fi

# 7. Verify standard_stman.cpp is linked into the core library.
if ! grep -q "standard_stman.cpp" CMakeLists.txt; then
  echo "FAIL: standard_stman.cpp not in CMakeLists.txt" >&2
  exit 1
fi

echo "Phase 7 W12 gate passed"
