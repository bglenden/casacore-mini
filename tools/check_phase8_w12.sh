#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-phase8}"

echo "=== Phase 8 Wave 12 Gate: Closeout ==="

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

# --- Exit report exists ---
check "docs/phase8/exit_report.md exists" \
  test -f "${ROOT_DIR}/docs/phase8/exit_report.md"

# --- Plan status is Complete ---
if grep -q 'Status: Complete' "${ROOT_DIR}/docs/phase8/plan.md" 2>/dev/null; then
  echo "  PASS: plan.md status is Complete"
  PASS=$((PASS + 1))
else
  echo "  FAIL: plan.md status is not Complete" >&2
  FAIL=$((FAIL + 1))
fi

# --- casacore_plan.md updated ---
if grep -q 'Phase 8 (complete' "${ROOT_DIR}/docs/casacore_plan.md" 2>/dev/null; then
  echo "  PASS: casacore_plan.md Phase 8 marked complete"
  PASS=$((PASS + 1))
else
  echo "  FAIL: casacore_plan.md Phase 8 not marked complete" >&2
  FAIL=$((FAIL + 1))
fi

# --- All unit tests pass ---
if ctest --test-dir "${ROOT_DIR}/${BUILD_DIR}" --output-on-failure 2>&1 \
   | grep -q "100% tests passed"; then
  echo "  PASS: all unit tests pass"
  PASS=$((PASS + 1))
else
  echo "  FAIL: unit tests did not all pass" >&2
  FAIL=$((FAIL + 1))
fi

# --- Known differences documented ---
check "docs/phase8/known_differences.md exists" \
  test -f "${ROOT_DIR}/docs/phase8/known_differences.md"

echo ""
echo "=== W12 gate: PASS=${PASS} FAIL=${FAIL} ==="
if [ "${FAIL}" -gt 0 ]; then
  echo "W12 gate FAILED" >&2
  exit 1
fi
echo "W12 gate passed."
