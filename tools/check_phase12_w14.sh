#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-closeout}"

cd "${ROOT_DIR}"

echo "=== P12-W14 wave gate ==="

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

# Fresh build from clean directory.
cmake -S . -B "${BUILD_DIR}" \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=OFF >/dev/null 2>&1

check "fresh-build" cmake --build "${BUILD_DIR}" -j8
check "full-test-suite" ctest --test-dir "${BUILD_DIR}" --output-on-failure -j8

# All wave gates pass.
for w in $(seq 1 13); do
  check "wave-${w}-gate" bash tools/check_phase12_w${w}.sh
done

# Real interop matrix checks on casacore-enabled hosts.
if pkg-config --exists casacore 2>/dev/null; then
  check "phase8-matrix" bash tools/interop/run_phase8_matrix.sh "${BUILD_DIR}"
  check "phase10-matrix" bash tools/interop/run_phase10_matrix.sh "${BUILD_DIR}"
  check "phase11-matrix" bash tools/interop/run_phase11_matrix.sh "${BUILD_DIR}"
else
  echo "  SKIP: casacore not available for interop matrix checks"
fi

# Review artifacts exist for all waves.
for w in $(seq 1 14); do
  check "review-P12-W${w}" test -d docs/phase12/review/P12-W${w}
done

# Exit report exists.
check "exit-report" test -f docs/phase12/exit_report.md

# Plan workstream table is updated.
check "plan-updated" sh -c 'grep -q "Done" docs/phase12/plan.md'

echo ""
echo "=== P12-W14 gate summary ==="
echo "PASS: ${PASS}"
echo "FAIL: ${FAIL}"

if [ "${FAIL}" -gt 0 ]; then
  echo "P12-W14 gate FAILED" >&2
  exit 1
fi

echo "P12-W14 gate passed"
