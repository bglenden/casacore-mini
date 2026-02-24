#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 W9 gate: acceptance suite integration"

# The aggregate suite must exist and pass.
if [ ! -f "tools/check_phase7.sh" ]; then
  echo "FAIL: tools/check_phase7.sh not found" >&2
  exit 1
fi

bash tools/check_phase7.sh "${BUILD_DIR}"

# Verify the CI script uses the aggregate rather than individual wave checks.
if grep -q "check_phase7_w3\|check_phase7_w4\|check_phase7_w5\|check_phase7_w6\|check_phase7_w7\|check_phase7_w8" tools/check_ci_build_lint_test_coverage.sh; then
  echo "FAIL: CI script still has individual wave checks (should use check_phase7.sh)" >&2
  exit 1
fi

if ! grep -q "check_phase7.sh" tools/check_ci_build_lint_test_coverage.sh; then
  echo "FAIL: CI script does not reference check_phase7.sh" >&2
  exit 1
fi

echo "Phase 7 W9 gate passed"
