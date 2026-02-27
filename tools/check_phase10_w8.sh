#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W8 gate: mutation flows and persistence integrity"

# ── Writable open support ────────────────────────────────────────────
echo "Checking writable open support..."
grep -q "bool writable" include/casacore_mini/lattice.hpp \
    || { echo "FAIL: writable param not in PagedArray"; exit 1; }
grep -q "bool writable" include/casacore_mini/image.hpp \
    || { echo "FAIL: writable param not in PagedImage"; exit 1; }
echo "  Writable open parameters present."

# ── TSM writer extensions ────────────────────────────────────────────
echo "Checking TSM writer double/complex/read-back methods..."
for sym in write_double_cell write_complex_cell read_float_cell \
           read_double_cell read_raw_cell has_column find_column; do
    grep -q "$sym" include/casacore_mini/tiled_stman.hpp \
        || { echo "FAIL: $sym not in tiled_stman.hpp"; exit 1; }
done
echo "  All TSM writer extensions present."

# ── Build ────────────────────────────────────────────────────────────
echo "Building..."
cmake --build "${BUILD_DIR}" --parallel 2>&1 | tail -3

# ── Unit test ────────────────────────────────────────────────────────
echo "Running mutation_persist_test..."
ctest --test-dir "${BUILD_DIR}" -R mutation_persist_test --output-on-failure 2>&1

# ── Full regression ──────────────────────────────────────────────────
echo "Running full regression..."
RESULT=$(ctest --test-dir "${BUILD_DIR}" --output-on-failure 2>&1)
echo "$RESULT" | tail -3
if echo "$RESULT" | grep -q "tests passed.*0 tests failed"; then
    echo "P10-W8 gate: PASS"
else
    echo "P10-W8 gate: FAIL"
    exit 1
fi
