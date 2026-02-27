#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W6 gate: expression engine foundation"

# ── Header declarations ──────────────────────────────────────────────
echo "Checking lattice_expr.hpp declarations..."
for sym in LelType LelResult LelNode LelScalar LelArrayRef LelBinary \
           LelCompare LelUnary LelFunc1 LelFunc2 LelReduce LelBoolReduce \
           LelIif LatticeExprNode LatticeExpr; do
    grep -q "$sym" include/casacore_mini/lattice_expr.hpp \
        || { echo "FAIL: $sym not found in lattice_expr.hpp"; exit 1; }
done
echo "  All required symbols present."

# ── Builder helpers ──────────────────────────────────────────────────
echo "Checking builder helper functions..."
for fn in lel_scalar lel_array lel_add lel_sub lel_mul lel_div \
          lel_compare lel_negate lel_not lel_and lel_or lel_iif \
          lel_math1 lel_math2 lel_reduce; do
    grep -q "$fn" include/casacore_mini/lattice_expr.hpp \
        || { echo "FAIL: helper $fn not found in lattice_expr.hpp"; exit 1; }
done
echo "  All builder helpers present."

# ── Build ────────────────────────────────────────────────────────────
echo "Building..."
cmake --build "${BUILD_DIR}" --parallel 2>&1 | tail -3

# ── Unit test ────────────────────────────────────────────────────────
echo "Running lattice_expr_test..."
ctest --test-dir "${BUILD_DIR}" -R lattice_expr_test --output-on-failure 2>&1

# ── Full regression ──────────────────────────────────────────────────
echo "Running full regression..."
RESULT=$(ctest --test-dir "${BUILD_DIR}" --output-on-failure 2>&1)
echo "$RESULT" | tail -3
if echo "$RESULT" | grep -q "tests passed.*0 tests failed"; then
    echo "P10-W6 gate: PASS"
else
    echo "P10-W6 gate: FAIL"
    exit 1
fi
