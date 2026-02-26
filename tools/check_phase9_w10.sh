#!/usr/bin/env bash
# Phase 9 Wave 10 gate: Mutation operations.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W10 gate ==="

# 1. Required source files exist.
for f in include/casacore_mini/ms_concat.hpp \
         include/casacore_mini/ms_flagger.hpp \
         src/ms_concat.cpp \
         src/ms_flagger.cpp \
         tests/ms_concat_test.cpp \
         tests/ms_flagger_test.cpp; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. Build succeeds.
cmake --build "$BUILD" --target ms_concat_test --target ms_flagger_test 2>&1 | tail -1
echo "  build: OK"

# 3. Tests pass.
ctest --test-dir "$BUILD" -R "ms_concat_test|ms_flagger_test" --output-on-failure
echo "  tests: OK"

echo "=== P9-W10 gate PASSED ==="
