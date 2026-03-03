#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build-phase9}"
CXX_BIN="${CXX:-clang++}"

cd "${ROOT_DIR}"

# Phase 9 interop matrix: 5 artifacts x 4 directions = 20 cells.

echo "=== Phase 9 interoperability matrix ==="

PASS_COUNT=0
FAIL_COUNT=0

record_pass() {
  local label="$1"
  echo "  PASS: ${label}"
  PASS_COUNT=$((PASS_COUNT + 1))
}

record_fail() {
  local label="$1"
  echo "  FAIL: ${label}" >&2
  FAIL_COUNT=$((FAIL_COUNT + 1))
}

# Check prerequisites.
if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config is required" >&2
  exit 1
fi

if ! pkg-config --exists casacore; then
  echo "casacore development package not found via pkg-config" >&2
  exit 1
fi

# Build tools.
cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF

cmake --build "${BUILD_DIR}" --target interop_mini_tool

CASACORE_TOOL="${BUILD_DIR}/casacore_interop_tool"
"${CXX_BIN}" -std=c++20 -Wall -Wextra -Wpedantic -Werror \
  $(pkg-config --cflags casacore) \
  "${ROOT_DIR}/tools/interop/casacore_interop_tool.cpp" \
  -o "${CASACORE_TOOL}" \
  $(pkg-config --libs casacore)

MINI_TOOL="${BUILD_DIR}/interop_mini_tool"
ARTIFACT_DIR="${BUILD_DIR}/phase9_artifacts"
rm -rf "${ARTIFACT_DIR}"
mkdir -p "${ARTIFACT_DIR}"

# --- Artifact matrix cells ---
run_cell() {
  local artifact="$1"
  local producer_tool="$2"
  local verifier_tool="$3"
  local producer_name="$4"
  local verifier_name="$5"

  local label="${artifact}: ${producer_name}->${verifier_name}"
  local outdir="${ARTIFACT_DIR}/${producer_name}_${artifact}"

  if "${producer_tool}" "produce-${artifact}" --output "${outdir}" 2>/dev/null \
     && "${verifier_tool}" "verify-${artifact}" --input "${outdir}" 2>/dev/null; then
    record_pass "${label}"
  else
    record_fail "${label}"
  fi
}

ARTIFACTS=(ms-minimal ms-representative ms-optional-subtables ms-concat ms-selection-stress)

for art in "${ARTIFACTS[@]}"; do
  run_cell "${art}" "${MINI_TOOL}"     "${MINI_TOOL}"     "mini"     "mini"
  run_cell "${art}" "${MINI_TOOL}"     "${CASACORE_TOOL}" "mini"     "casacore"
  run_cell "${art}" "${CASACORE_TOOL}" "${MINI_TOOL}"     "casacore" "mini"
  run_cell "${art}" "${CASACORE_TOOL}" "${CASACORE_TOOL}" "casacore" "casacore"
done

echo ""
echo "=== Phase 9 interoperability matrix summary ==="
echo "PASS: ${PASS_COUNT}"
echo "FAIL: ${FAIL_COUNT}"

if [ "${FAIL_COUNT}" -gt 0 ]; then
  echo "Phase 9 interoperability matrix FAILED (${FAIL_COUNT} failures)" >&2
  exit 1
fi

echo "Phase 9 interoperability matrix passed (${PASS_COUNT}/20)"
