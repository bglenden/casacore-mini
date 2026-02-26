#!/usr/bin/env bash
# Phase 9 Wave 9 gate: Read/introspection operations.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W9 gate ==="

# 1. Required source files exist.
for f in include/casacore_mini/ms_metadata.hpp \
         include/casacore_mini/ms_summary.hpp \
         src/ms_metadata.cpp \
         src/ms_summary.cpp \
         tests/ms_metadata_test.cpp \
         tests/ms_summary_test.cpp; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. Build succeeds.
cmake --build "$BUILD" --target ms_metadata_test --target ms_summary_test 2>&1 | tail -1
echo "  build: OK"

# 3. Tests pass.
ctest --test-dir "$BUILD" -R "ms_metadata_test|ms_summary_test" --output-on-failure
echo "  tests: OK"

echo "=== P9-W9 gate PASSED ==="
