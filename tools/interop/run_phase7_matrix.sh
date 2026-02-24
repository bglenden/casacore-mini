#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build-phase7}"
GENERATOR="${GENERATOR:-Ninja}"
CXX_BIN="${CXX:-clang++}"

cd "${ROOT_DIR}"

if ! command -v pkg-config >/dev/null 2>&1; then
  echo "pkg-config is required" >&2
  exit 1
fi

if ! pkg-config --exists casacore; then
  echo "casacore development package not found via pkg-config" >&2
  echo "Install on macOS: brew tap casacore/tap && brew install casacore" >&2
  exit 1
fi

cmake -S . -B "${BUILD_DIR}" -G "${GENERATOR}" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
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
ARTIFACT_DIR="${BUILD_DIR}/phase7_artifacts"
rm -rf "${ARTIFACT_DIR}"
mkdir -p "${ARTIFACT_DIR}"

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

verify_both() {
  local verify_cmd="$1"
  local dump_cmd="$2"
  local input_path="$3"
  local stem="$4"

  if ! "${MINI_TOOL}" "${verify_cmd}" --input "${input_path}"; then
    "${MINI_TOOL}" "${dump_cmd}" --input "${input_path}" --output "${ARTIFACT_DIR}/${stem}.mini.dump.txt" || true
    "${CASACORE_TOOL}" "${dump_cmd}" --input "${input_path}" --output "${ARTIFACT_DIR}/${stem}.casacore.dump.txt" || true
    echo "mini verification failed for ${input_path}" >&2
    exit 1
  fi

  if ! "${CASACORE_TOOL}" "${verify_cmd}" --input "${input_path}"; then
    "${MINI_TOOL}" "${dump_cmd}" --input "${input_path}" --output "${ARTIFACT_DIR}/${stem}.mini.dump.txt" || true
    "${CASACORE_TOOL}" "${dump_cmd}" --input "${input_path}" --output "${ARTIFACT_DIR}/${stem}.casacore.dump.txt" || true
    echo "casacore verification failed for ${input_path}" >&2
    exit 1
  fi
}

run_case() {
  local case_name="$1"
  local write_cmd="$2"
  local verify_cmd="$3"
  local dump_cmd="$4"

  local casacore_artifact="${ARTIFACT_DIR}/${case_name}.producer_casacore.bin"
  local mini_artifact="${ARTIFACT_DIR}/${case_name}.producer_mini.bin"

  "${CASACORE_TOOL}" "${write_cmd}" --output "${casacore_artifact}"
  verify_both "${verify_cmd}" "${dump_cmd}" "${casacore_artifact}" "${case_name}.from_casacore"

  "${MINI_TOOL}" "${write_cmd}" --output "${mini_artifact}"
  verify_both "${verify_cmd}" "${dump_cmd}" "${mini_artifact}" "${case_name}.from_mini"
}

run_dir_case() {
  local case_name="$1"
  local write_cmd="$2"
  local verify_cmd="$3"
  local dump_cmd="$4"

  local casacore_dir="${ARTIFACT_DIR}/${case_name}.producer_casacore"
  local mini_dir="${ARTIFACT_DIR}/${case_name}.producer_mini"

  rm -rf "${casacore_dir}" "${mini_dir}"

  echo "--- ${case_name} ---"

  # Cell 1: casacore produces, casacore verifies.
  "${CASACORE_TOOL}" "${write_cmd}" --output "${casacore_dir}"
  if "${CASACORE_TOOL}" "${verify_cmd}" --input "${casacore_dir}"; then
    record_pass "${case_name}: casacore->casacore"
  else
    "${CASACORE_TOOL}" "${dump_cmd}" --input "${casacore_dir}" --output "${ARTIFACT_DIR}/${case_name}.from_casacore.casacore.dump.txt" || true
    record_fail "${case_name}: casacore->casacore"
  fi

  # Cell 2: casacore produces, mini verifies.
  if "${MINI_TOOL}" "${verify_cmd}" --input "${casacore_dir}"; then
    record_pass "${case_name}: casacore->mini"
  else
    "${MINI_TOOL}" "${dump_cmd}" --input "${casacore_dir}" --output "${ARTIFACT_DIR}/${case_name}.from_casacore.mini.dump.txt" || true
    record_fail "${case_name}: casacore->mini"
  fi

  # Cell 3: mini produces, mini verifies.
  "${MINI_TOOL}" "${write_cmd}" --output "${mini_dir}"
  if "${MINI_TOOL}" "${verify_cmd}" --input "${mini_dir}"; then
    record_pass "${case_name}: mini->mini"
  else
    "${MINI_TOOL}" "${dump_cmd}" --input "${mini_dir}" --output "${ARTIFACT_DIR}/${case_name}.from_mini.mini.dump.txt" || true
    record_fail "${case_name}: mini->mini"
  fi

  # Cell 4: mini produces, casacore verifies (strict — no non-fatal bypass).
  if "${CASACORE_TOOL}" "${verify_cmd}" --input "${mini_dir}"; then
    record_pass "${case_name}: mini->casacore"
  else
    "${CASACORE_TOOL}" "${dump_cmd}" --input "${mini_dir}" --output "${ARTIFACT_DIR}/${case_name}.from_mini.casacore.dump.txt" || true
    record_fail "${case_name}: mini->casacore"
  fi
}

run_case "record_basic" "write-record-basic" "verify-record-basic" "dump-record"
run_case "record_nested" "write-record-nested" "verify-record-nested" "dump-record"
run_case "record_fixture_logtable_time" "write-record-fixture-logtable-time" "verify-record-fixture-logtable-time" "dump-record"
run_case "record_fixture_ms_table" "write-record-fixture-ms-table" "verify-record-fixture-ms-table" "dump-record"
run_case "record_fixture_ms_uvw" "write-record-fixture-ms-uvw" "verify-record-fixture-ms-uvw" "dump-record"
run_case "record_fixture_pagedimage_table" "write-record-fixture-pagedimage-table" "verify-record-fixture-pagedimage-table" "dump-record"
run_case "table_dat_header" "write-table-dat-header" "verify-table-dat-header" "dump-table-dat-header"
run_case "table_dat_full" "write-table-dat-full" "verify-table-dat-full" "dump-table-dat-full"
run_dir_case "table_dir" "write-table-dir" "verify-table-dir" "dump-table-dir"
run_dir_case "ism_dir" "write-ism-dir" "verify-ism-dir" "dump-table-dir"
run_dir_case "tiled_col_dir" "write-tiled-col-dir" "verify-tiled-col-dir" "dump-table-dir"
run_dir_case "tiled_cell_dir" "write-tiled-cell-dir" "verify-tiled-cell-dir" "dump-table-dir"
run_dir_case "tiled_shape_dir" "write-tiled-shape-dir" "verify-tiled-shape-dir" "dump-table-dir"
run_dir_case "tiled_data_dir" "write-tiled-data-dir" "verify-tiled-data-dir" "dump-table-dir"

echo ""
echo "=== Phase 7 interoperability matrix summary ==="
echo "PASS: ${PASS_COUNT}"
echo "FAIL: ${FAIL_COUNT}"

if [ "${FAIL_COUNT}" -gt 0 ]; then
  echo "Phase 7 interoperability matrix FAILED (${FAIL_COUNT} failures)" >&2
  exit 1
fi

echo "Phase 7 interoperability matrix passed"
