#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Gate script for Phase 11, Wave 8: Integration convergence
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W8 Gate: Integration convergence ==="

# 1. Integration test exists and registered
echo "--- Integration test ---"
[[ -f "$ROOT/tests/phase11_integration_test.cpp" ]] && pass "integration test exists" || fail "missing"
grep -q 'phase11_integration_test' "$ROOT/CMakeLists.txt" && pass "integration test in CMakeLists" || fail "not in CMakeLists"

# 2. Build and run integration test + all prior tests
echo "--- Build & Test ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
    cmake --build "$BUILD" --target phase11_integration_test taql_w7_closure_test taql_eval_test taql_command_test ms_selection_table_expr_bridge_test ms_selection_parser_test ms_selection_malformed_test ms_selection_test ms_selection_parity_test 2>&1 | tail -2
    "$BUILD/phase11_integration_test" 2>&1 | grep -q '0 failed' && pass "integration_test passes" || fail "integration_test fails"
    "$BUILD/taql_w7_closure_test" 2>&1 | grep -q '0 failed' && pass "taql_w7_closure_test" || fail "taql_w7 regression"
    "$BUILD/taql_eval_test" 2>&1 | grep -q '0 failed' && pass "taql_eval_test" || fail "taql_eval regression"
    "$BUILD/taql_command_test" 2>&1 | grep -q '0 failed' && pass "taql_command_test" || fail "taql_cmd regression"
    "$BUILD/ms_selection_table_expr_bridge_test" 2>&1 | grep -q '0 failed' && pass "bridge_test" || fail "bridge regression"
    "$BUILD/ms_selection_parser_test" 2>&1 | grep -q '0 failed' && pass "parser_test" || fail "parser regression"
    "$BUILD/ms_selection_malformed_test" 2>&1 | grep -q '0 failed' && pass "malformed_test" || fail "malformed regression"
    "$BUILD/ms_selection_test" 2>&1 | grep -q 'all.*passed' && pass "ms_selection_test" || fail "ms_sel regression"
    "$BUILD/ms_selection_parity_test" 2>&1 | grep -q 'all.*passed' && pass "parity_test" || fail "parity regression"
else
    fail "build directory not found"
fi

# 3. API consistency
echo "--- API consistency ---"
HDR="$ROOT/include/casacore_mini/ms_selection.hpp"
grep -q 'to_taql_where' "$HDR" && pass "to_taql_where in API" || fail "to_taql_where missing"
grep -q 'has_selection' "$HDR" && pass "has_selection in API" || fail "has_selection missing"
grep -q 'evaluate' "$HDR" && pass "evaluate in API" || fail "evaluate missing"

echo ""
echo "=== P11-W8 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
