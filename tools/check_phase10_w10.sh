#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"
CXX_BIN="${CXX:-clang++}"

cd "${ROOT_DIR}"

echo "Phase 10 W10 gate: strict image/lattice interoperability matrix"

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "FAIL: pkg-config is required for W10 interop gate" >&2
  exit 1
fi

if ! pkg-config --exists casacore; then
  echo "FAIL: casacore development package not found via pkg-config" >&2
  exit 1
fi

echo "Building interop tools..."
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

if [ ! -x "${MINI_TOOL}" ]; then
  echo "FAIL: interop_mini_tool was not built at ${MINI_TOOL}" >&2
  exit 1
fi
if [ ! -x "${CASACORE_TOOL}" ]; then
  echo "FAIL: casacore_interop_tool was not built at ${CASACORE_TOOL}" >&2
  exit 1
fi

echo "Running full W10 interop matrix (5 artifacts x 4 directions)..."
MATRIX_LOG="${BUILD_DIR}/phase10_w10_matrix.log"
mkdir -p "${BUILD_DIR}"
rm -f "${MATRIX_LOG}"
if ! bash tools/interop/run_phase10_matrix.sh "${BUILD_DIR}" | tee "${MATRIX_LOG}"; then
  echo "FAIL: Phase 10 interop matrix execution failed" >&2
  exit 1
fi

if ! grep -q "^  PASS: 20/20$" "${MATRIX_LOG}"; then
  echo "FAIL: W10 requires exact matrix result PASS 20/20" >&2
  exit 1
fi
if ! grep -q "^  FAIL: 0/20$" "${MATRIX_LOG}"; then
  echo "FAIL: W10 requires exact matrix result FAIL 0/20" >&2
  exit 1
fi

echo "Running explicit metadata contract checks..."
META_DIR="${BUILD_DIR}/phase10_w10_metadata"
rm -rf "${META_DIR}"
mkdir -p "${META_DIR}"

# img-minimal: units must survive casacore->mini interoperability.
"${CASACORE_TOOL}" produce-img-minimal --output "${META_DIR}/img-minimal-casa"
"${MINI_TOOL}" verify-img-minimal --input "${META_DIR}/img-minimal-casa"
echo "  PASS: metadata contract img-minimal (units)"

# img-region-subset: region misc_info keys must survive casacore->mini.
"${CASACORE_TOOL}" produce-img-region-subset --output "${META_DIR}/img-region-subset-casa"
"${MINI_TOOL}" verify-img-region-subset --input "${META_DIR}/img-region-subset-casa"
echo "  PASS: metadata contract img-region-subset (region misc_info)"

# img-expression: units + expression misc_info must survive casacore->mini.
"${CASACORE_TOOL}" produce-img-expression --output "${META_DIR}/img-expression-casa"
"${MINI_TOOL}" verify-img-expression --input "${META_DIR}/img-expression-casa"
echo "  PASS: metadata contract img-expression (units + expression misc_info)"

echo "P10-W10 gate: PASS"
