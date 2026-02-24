#!/usr/bin/env bash
# Phase 7 W17 gate: full hardening — all wave gates + build + tests + strict matrix
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

PASS=0; FAIL=0
check() {
  local desc="$1"; shift
  if "$@" >/dev/null 2>&1; then
    echo "  PASS  $desc"; PASS=$((PASS + 1))
  else
    echo "  FAIL  $desc"; FAIL=$((FAIL + 1))
  fi
}

echo "=== Phase 7 W17: Hardening ==="

# 1. All previous wave gates pass
echo "--- Wave gate chain ---"
check "W11 gate passes" bash tools/check_phase7_w11.sh "${BUILD_DIR}"
check "W12 gate passes" bash tools/check_phase7_w12.sh "${BUILD_DIR}"
check "W13 gate passes" bash tools/check_phase7_w13.sh "${BUILD_DIR}"
check "W14 gate passes" bash tools/check_phase7_w14.sh "${BUILD_DIR}"
check "W15 gate passes" bash tools/check_phase7_w15.sh "${BUILD_DIR}"
check "W16 gate passes" bash tools/check_phase7_w16.sh "${BUILD_DIR}"

# 2. Full build with clang-tidy passes (if build dir exists)
echo "--- Build and test ---"
if [ -d "${BUILD_DIR}" ]; then
  check "Full build passes" cmake --build "${BUILD_DIR}" --target all
  ctest_log="${BUILD_DIR}/phase7_w17_ctest.log"
  if ctest --test-dir "${BUILD_DIR}" --output-on-failure >"${ctest_log}" 2>&1; then
    echo "  PASS  All ctest tests pass"; PASS=$((PASS + 1))
  else
    echo "  FAIL  All ctest tests pass"; FAIL=$((FAIL + 1))
    echo "  ---- ctest output (phase7_w17) ----"
    cat "${ctest_log}"
    echo "  ---- end ctest output ----"
  fi
fi

# 3. Interop matrix script is available and has all 6 SM directory cases
MATRIX_SCRIPT="tools/interop/run_phase7_matrix.sh"
echo "--- Interop matrix structure ---"
check "Matrix script exists"          test -f "${MATRIX_SCRIPT}"
check "Matrix has table_dir case"     grep -q "run_dir_case.*table_dir"       "${MATRIX_SCRIPT}"
check "Matrix has ism_dir case"       grep -q "run_dir_case.*ism_dir"         "${MATRIX_SCRIPT}"
check "Matrix has tiled_col_dir case" grep -q "run_dir_case.*tiled_col_dir"   "${MATRIX_SCRIPT}"
check "Matrix has tiled_cell_dir case" grep -q "run_dir_case.*tiled_cell_dir" "${MATRIX_SCRIPT}"
check "Matrix has tiled_shape_dir case" grep -q "run_dir_case.*tiled_shape_dir" "${MATRIX_SCRIPT}"
check "Matrix has tiled_data_dir case" grep -q "run_dir_case.*tiled_data_dir" "${MATRIX_SCRIPT}"
check "Matrix exits non-zero on failure" grep -q "exit 1" "${MATRIX_SCRIPT}"

# 4. Verify check_phase7.sh wires the matrix as a gating check (not informational)
echo "--- Full acceptance suite wiring ---"
FULL_SUITE="tools/check_phase7.sh"
check "Acceptance suite includes W12 gate"  grep -q "check_phase7_w12.sh" "${FULL_SUITE}"
check "Acceptance suite includes W13 gate"  grep -q "check_phase7_w13.sh" "${FULL_SUITE}"
check "Acceptance suite includes W14 gate"  grep -q "check_phase7_w14.sh" "${FULL_SUITE}"
check "Acceptance suite includes W15 gate"  grep -q "check_phase7_w15.sh" "${FULL_SUITE}"
check "Acceptance suite includes W16 gate"  grep -q "check_phase7_w16.sh" "${FULL_SUITE}"
check "Acceptance suite includes W17 gate"  grep -q "check_phase7_w17.sh" "${FULL_SUITE}"
check "Acceptance suite matrix is gating (no non-fatal bypass)" \
  bash -c "! grep -q 'FAILURES DETECTED (not yet gating CI)' '${FULL_SUITE}'"

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
