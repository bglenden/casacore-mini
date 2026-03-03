#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 9 W2 gate: MeasurementSet core model and lifecycle"

# 1. Verify required header and source files exist.
REQUIRED_FILES=(
  "include/casacore_mini/ms_enums.hpp"
  "include/casacore_mini/measurement_set.hpp"
  "src/ms_enums.cpp"
  "src/measurement_set.cpp"
  "tests/measurement_set_test.cpp"
)

for f in "${REQUIRED_FILES[@]}"; do
  if [ ! -s "${f}" ]; then
    echo "FAIL: ${f} missing or empty" >&2
    exit 1
  fi
done

# 2. Verify CMakeLists.txt includes the new sources.
if ! grep -q "ms_enums.cpp" CMakeLists.txt; then
  echo "FAIL: CMakeLists.txt missing ms_enums.cpp" >&2
  exit 1
fi
if ! grep -q "measurement_set.cpp" CMakeLists.txt; then
  echo "FAIL: CMakeLists.txt missing measurement_set.cpp" >&2
  exit 1
fi
if ! grep -q "measurement_set_test" CMakeLists.txt; then
  echo "FAIL: CMakeLists.txt missing measurement_set_test" >&2
  exit 1
fi

# 3. Build (if build dir provided).
if [ -d "${BUILD_DIR}" ]; then
  cmake --build "${BUILD_DIR}" --target measurement_set_test 2>&1
fi

# 4. Run the test (if build dir has the binary).
if [ -x "${BUILD_DIR}/measurement_set_test" ]; then
  "${BUILD_DIR}/measurement_set_test"
fi

# 5. Verify ms_enums.hpp defines required column enum.
if ! grep -q "MsMainColumn" include/casacore_mini/ms_enums.hpp; then
  echo "FAIL: ms_enums.hpp missing MsMainColumn enum" >&2
  exit 1
fi

# 6. Verify measurement_set.hpp defines MeasurementSet class.
if ! grep -q "class MeasurementSet" include/casacore_mini/measurement_set.hpp; then
  echo "FAIL: measurement_set.hpp missing MeasurementSet class" >&2
  exit 1
fi

echo "Phase 9 W2 gate passed"
