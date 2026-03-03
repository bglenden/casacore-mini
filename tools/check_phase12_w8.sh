#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P12-W8 wave gate: MSSelection parser/evaluator closure
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=${1:-build}
PASS=0; FAIL=0
run() { if eval "$1"; then ((++PASS)); else ((++FAIL)); echo "  FAIL: $2"; fi; }
SRC="$SCRIPT_DIR/src/ms_selection.cpp"

echo "=== P12-W8 wave gate ==="

# 1. Build succeeds
run "cmake --build '$BUILD' --target ms_selection_parity_extended_test >/dev/null 2>&1" "build"

# 2. All existing tests pass
run "ctest --test-dir '$BUILD' --output-on-failure -j4 >/dev/null 2>&1" "tests"

# 3. New extended parity test passes
run "'$BUILD/ms_selection_parity_extended_test' >/dev/null 2>&1" "parity-extended-test"

# 4. Antenna && baseline support implemented
run "grep -q 'auto_only' '$SRC'" "antenna-auto-baseline"

# 5. Regex pattern support (std::regex)
run "grep -q '#include <regex>' '$SRC'" "regex-support"

# 6. Time string parser
run "grep -q 'parse_time_string' '$SRC'" "time-string-parser"

# 7. UV unit handling
run "grep -q 'parse_uv_with_unit' '$SRC'" "uv-unit-handling"

# 8. SPW frequency selection
run "grep -q 'freq_mult' '$SRC'" "spw-frequency-selection"

# 9. State bounds
run "grep -q 'State: invalid bound' '$SRC'" "state-bounds"

# 10. Field negation
run "grep -q 'field_negate' '$SRC'" "field-negation"

echo ""
echo "=== P12-W8 gate summary ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -eq 0 ]; then echo "P12-W8 gate passed"; else echo "P12-W8 gate FAILED"; exit 1; fi
