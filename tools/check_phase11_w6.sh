#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Gate script for Phase 11, Wave 6: MS selection evaluator + to_table_expr_node bridge
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W6 Gate: MS selection evaluator + to_table_expr_node bridge ==="

# 1. to_taql_where() API
echo "--- API surface ---"
HDR="$ROOT/include/casacore_mini/ms_selection.hpp"
grep -q 'to_taql_where' "$HDR" && pass "API: to_taql_where" || fail "API: to_taql_where missing"

# 2. Expression accessor methods (12)
echo "--- Expression accessors ---"
for fn in antenna_expr field_expr spw_expr scan_expr time_expr uvdist_expr corr_expr state_expr observation_expr array_expr feed_expr taql_expr; do
    grep -q "${fn}() const" "$HDR" && pass "accessor: $fn" || fail "accessor: $fn missing"
done

# 3. Source has to_taql_where implementation
echo "--- Implementation ---"
SRC="$ROOT/src/ms_selection.cpp"
grep -q 'to_taql_where' "$SRC" && pass "impl: to_taql_where" || fail "impl: to_taql_where missing"
grep -q 'int_expr_to_taql' "$SRC" && pass "impl: int_expr_to_taql helper" || fail "impl: int_expr_to_taql missing"
grep -q 'bound_to_taql' "$SRC" && pass "impl: bound_to_taql helper" || fail "impl: bound_to_taql missing"

# 4. Test file exists and registered
echo "--- Test files ---"
[[ -f "$ROOT/tests/ms_selection_table_expr_bridge_test.cpp" ]] && pass "bridge test exists" || fail "bridge test missing"
grep -q 'ms_selection_table_expr_bridge_test' "$ROOT/CMakeLists.txt" && pass "bridge test in CMakeLists" || fail "bridge test not in CMakeLists"

# 5. Build and run tests
echo "--- Build & Test ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
    cmake --build "$BUILD" --target ms_selection_table_expr_bridge_test ms_selection_parser_test ms_selection_malformed_test ms_selection_test ms_selection_parity_test 2>&1 | tail -2
    "$BUILD/ms_selection_table_expr_bridge_test" 2>&1 | grep -q '0 failed' && pass "bridge_test passes" || fail "bridge_test fails"
    "$BUILD/ms_selection_parser_test" 2>&1 | grep -q '0 failed' && pass "parser_test passes" || fail "parser_test fails"
    "$BUILD/ms_selection_malformed_test" 2>&1 | grep -q '0 failed' && pass "malformed_test passes" || fail "malformed_test fails"
    "$BUILD/ms_selection_test" 2>&1 | grep -q 'all.*passed' && pass "existing ms_selection_test passes" || fail "regression in ms_selection_test"
    "$BUILD/ms_selection_parity_test" 2>&1 | grep -q 'all.*passed' && pass "existing parity_test passes" || fail "regression in parity_test"
else
    fail "build directory not found"
fi

# 6. TaQL prerequisites
echo "--- Prerequisites ---"
"$BUILD/taql_eval_test" 2>&1 | grep -q '0 failed' && pass "taql_eval_test passes" || fail "taql_eval_test regression"
"$BUILD/taql_command_test" 2>&1 | grep -q '0 failed' && pass "taql_command_test passes" || fail "taql_command_test regression"

echo ""
echo "=== P11-W6 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
