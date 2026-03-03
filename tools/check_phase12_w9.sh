#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P12-W9 wave gate: MSSelection API/accessor/mode/error closure
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=${1:-build}
PASS=0; FAIL=0
run() { if eval "$1"; then ((++PASS)); else ((++FAIL)); echo "  FAIL: $2"; fi; }
SRC="$SCRIPT_DIR/src/ms_selection.cpp"
HDR="$SCRIPT_DIR/include/casacore_mini/ms_selection.hpp"

echo "=== P12-W9 wave gate ==="

# 1. Build succeeds
run "cmake --build '$BUILD' --target ms_selection_api_parity_test >/dev/null 2>&1" "build"

# 2. All existing tests pass
run "ctest --test-dir '$BUILD' --output-on-failure -j4 >/dev/null 2>&1" "tests"

# 3. New API parity test passes
run "'$BUILD/ms_selection_api_parity_test' >/dev/null 2>&1" "api-parity-test"

# 4. ParseMode enum exists in header
run "grep -q 'enum class ParseMode' '$HDR'" "parse-mode-enum"

# 5. ErrorMode enum exists in header
run "grep -q 'enum class ErrorMode' '$HDR'" "error-mode-enum"

# 6. TimeRange struct exists in header
run "grep -q 'struct TimeRange' '$HDR'" "time-range-struct"

# 7. UvRange struct exists in header
run "grep -q 'struct UvRange' '$HDR'" "uv-range-struct"

# 8. handle_error lambda in evaluate
run "grep -q 'handle_error' '$SRC'" "error-handler"

# 9. data_desc_ids populated
run "grep -q 'data_desc_ids' '$SRC'" "ddid-population"

# 10. feed1_list / feed2_list populated
run "grep -q 'feed1_list' '$SRC'" "feed-lists"

echo ""
echo "=== P12-W9 gate summary ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -eq 0 ]; then echo "P12-W9 gate passed"; else echo "P12-W9 gate FAILED"; exit 1; fi
