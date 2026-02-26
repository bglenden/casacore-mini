#!/usr/bin/env bash
# Phase 9 Wave 6 gate: Utility layer.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W6 gate ==="

# 1. Required source files exist.
for f in include/casacore_mini/ms_iter.hpp \
         include/casacore_mini/ms_util.hpp \
         src/ms_iter.cpp \
         src/ms_util.cpp \
         tests/ms_iter_test.cpp \
         tests/ms_util_test.cpp; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. Build succeeds.
cmake --build "$BUILD" --target ms_iter_test --target ms_util_test 2>&1 | tail -1
echo "  build: OK"

# 3. Tests pass.
ctest --test-dir "$BUILD" -R "ms_iter_test|ms_util_test" --output-on-failure
echo "  tests: OK"

echo "=== P9-W6 gate PASSED ==="
