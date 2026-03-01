#!/usr/bin/env bash
# Phase 12 aggregate gate: runs all wave gates.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-p12-aggregate}"

cd "${ROOT_DIR}"

echo "============================================"
echo "  Phase 12 Aggregate Gate"
echo "============================================"
echo ""

PASS=0
FAIL=0

run_wave() {
  local wave="$1"
  local script="tools/check_phase12_w${wave}.sh"
  if [[ ! -x "${script}" ]]; then
    echo "  SKIP: W${wave} gate script not executable (${script})"
    return
  fi
  echo "=== Running W${wave} gate ==="
  if bash "${script}" "${BUILD_DIR}"; then
    echo "  W${wave}: PASSED"
    PASS=$((PASS + 1))
  else
    echo "  W${wave}: FAILED"
    FAIL=$((FAIL + 1))
  fi
  echo ""
}

for w in $(seq 1 14); do
  run_wave "${w}"
done

echo "============================================"
echo "  Phase 12 Aggregate: pass=${PASS} fail=${FAIL}"
echo "============================================"

if [[ "${FAIL}" -gt 0 ]]; then
  exit 1
fi
echo "PHASE 12 AGGREGATE GATE PASSED"
