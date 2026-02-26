#!/usr/bin/env bash
# Phase 9 Wave 7 gate: Selection foundation.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W7 gate ==="

# 1. Required source files exist.
for f in include/casacore_mini/ms_selection.hpp \
         src/ms_selection.cpp \
         tests/ms_selection_test.cpp; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. Build succeeds.
cmake --build "$BUILD" --target ms_selection_test 2>&1 | tail -1
echo "  build: OK"

# 3. Tests pass.
ctest --test-dir "$BUILD" -R "ms_selection_test" --output-on-failure
echo "  tests: OK"

echo "=== P9-W7 gate PASSED ==="
