#!/usr/bin/env bash
# Phase 9 Wave 8 gate: Full selection-category behavior.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W8 gate ==="

# 1. Required files exist.
for f in tests/ms_selection_parity_test.cpp; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. Build succeeds.
cmake --build "$BUILD" --target ms_selection_test --target ms_selection_parity_test 2>&1 | tail -1
echo "  build: OK"

# 3. Tests pass.
ctest --test-dir "$BUILD" -R "ms_selection" --output-on-failure
echo "  tests: OK"

echo "=== P9-W8 gate PASSED ==="
