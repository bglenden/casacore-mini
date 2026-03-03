#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 W11 gate: strict interop gating and matrix truthfulness"

MATRIX_SCRIPT="tools/interop/run_phase7_matrix.sh"

# 1. Verify the matrix runner has no non-fatal bypass for mini->casacore.
if grep -q "expected: no SM data" "${MATRIX_SCRIPT}"; then
  echo "FAIL: matrix runner still has non-fatal bypass for mini->casacore" >&2
  exit 1
fi

if grep -q "EXPECTED_FAIL" "${MATRIX_SCRIPT}"; then
  echo "FAIL: matrix runner still has EXPECTED_FAIL handling" >&2
  exit 1
fi

# 2. Verify the matrix runner uses strict pass/fail counters.
if ! grep -q "PASS_COUNT" "${MATRIX_SCRIPT}"; then
  echo "FAIL: matrix runner missing pass counter" >&2
  exit 1
fi

if ! grep -q "FAIL_COUNT" "${MATRIX_SCRIPT}"; then
  echo "FAIL: matrix runner missing fail counter" >&2
  exit 1
fi

# 3. Verify the matrix runner exits non-zero on any failure.
if ! grep -q 'exit 1' "${MATRIX_SCRIPT}"; then
  echo "FAIL: matrix runner does not exit non-zero on failure" >&2
  exit 1
fi

# 4. Verify run_dir_case tests all four matrix cells explicitly.
for cell in "casacore->casacore" "casacore->mini" "mini->mini" "mini->casacore"; do
  if ! grep -q "${cell}" "${MATRIX_SCRIPT}"; then
    echo "FAIL: matrix runner missing explicit cell '${cell}'" >&2
    exit 1
  fi
done

# 5. Verify the aggregate acceptance suite runs the matrix conditionally.
AGGREGATE_SCRIPT="tools/check_phase7.sh"
if ! grep -q "run_phase7_matrix.sh" "${AGGREGATE_SCRIPT}"; then
  echo "FAIL: aggregate acceptance suite does not reference the matrix runner" >&2
  exit 1
fi

# 6. Verify the aggregate includes the W11 gate itself.
if ! grep -q "check_phase7_w11.sh" "${AGGREGATE_SCRIPT}"; then
  echo "FAIL: aggregate acceptance suite does not include W11 gate" >&2
  exit 1
fi

echo "Phase 7 W11 gate passed"
