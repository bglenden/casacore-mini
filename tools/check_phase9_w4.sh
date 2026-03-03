#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 9 Wave 4 gate: Typed column wrappers.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W4 gate ==="

# 1. Required source files exist.
for f in include/casacore_mini/ms_columns.hpp \
         src/ms_columns.cpp \
         tests/ms_columns_test.cpp; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. Build succeeds.
cmake --build "$BUILD" --target ms_columns_test 2>&1 | tail -1
echo "  build: OK"

# 3. Test passes.
ctest --test-dir "$BUILD" -R ms_columns_test --output-on-failure
echo "  tests: OK"

echo "=== P9-W4 gate PASSED ==="
