#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W12 gate: closeout and Phase-11 handoff"

FAIL=0

# 1. Exit report must exist.
if [[ ! -f "docs/phase10/exit_report.md" ]]; then
    echo "FAIL: docs/phase10/exit_report.md not found"
    FAIL=1
else
    echo "PASS: exit_report.md exists"
fi

# 2. All wave statuses must be Done in plan.md.
PENDING_WAVES=$(grep -cE '^\| `P10-W[0-9]+` \| Pending' docs/phase10/plan.md || true)
if [[ "${PENDING_WAVES}" -gt 0 ]]; then
    echo "FAIL: plan.md has ${PENDING_WAVES} pending waves"
    FAIL=1
else
    echo "PASS: all waves Done in plan.md"
fi

# 3. Rolling plan must show Phase 10 as complete.
if grep -q 'Phase 10 (complete)' docs/casacore_plan.md; then
    echo "PASS: Phase 10 marked complete in rolling plan"
else
    echo "FAIL: Phase 10 not marked complete in rolling plan"
    FAIL=1
fi

# 4. All tests must pass.
if ! ctest --test-dir "${BUILD_DIR}" --output-on-failure 2>&1 | tail -3 | grep -q '0 tests failed'; then
    echo "FAIL: not all tests passing"
    FAIL=1
else
    echo "PASS: all tests pass"
fi

if [[ "${FAIL}" -ne 0 ]]; then
    echo "P10-W12 gate: FAILED"
    exit 1
fi

echo "P10-W12 gate: ALL PASSED"
