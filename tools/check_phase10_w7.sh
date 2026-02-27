#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W7 gate: LEL parser and operator/function coverage"

# ── Parser and new node declarations ─────────────────────────────────
echo "Checking lattice_expr.hpp for W7 symbols..."
for sym in LelParseError LelSymbolTable lel_parse \
           LelValueExtract LelMaskExtract lel_value lel_mask \
           LelReal LelImag LelConj LelArg LelComplexAbs LelFormComplex \
           lel_real lel_imag lel_conj lel_arg lel_complex_abs lel_form_complex \
           LelPromoteFloat lel_promote_float lel_bool_reduce; do
    grep -q "$sym" include/casacore_mini/lattice_expr.hpp \
        || { echo "FAIL: $sym not found in lattice_expr.hpp"; exit 1; }
done
echo "  All required W7 symbols present."

# ── Build ────────────────────────────────────────────────────────────
echo "Building..."
cmake --build "${BUILD_DIR}" --parallel 2>&1 | tail -3

# ── Unit tests ───────────────────────────────────────────────────────
echo "Running lattice_expr_test..."
ctest --test-dir "${BUILD_DIR}" -R lattice_expr_test --output-on-failure 2>&1

echo "Running lel_parser_test..."
ctest --test-dir "${BUILD_DIR}" -R lel_parser_test --output-on-failure 2>&1

# ── Full regression ──────────────────────────────────────────────────
echo "Running full regression..."
RESULT=$(ctest --test-dir "${BUILD_DIR}" --output-on-failure 2>&1)
echo "$RESULT" | tail -3
if echo "$RESULT" | grep -q "tests passed.*0 tests failed"; then
    echo "P10-W7 gate: PASS"
else
    echo "P10-W7 gate: FAIL"
    exit 1
fi
