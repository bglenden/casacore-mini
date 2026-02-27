#!/usr/bin/env bash
# Gate script for Phase 11, Wave 7: TaQL + MS selection parity closure
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W7 Gate: TaQL + MS selection parity closure ==="

# 1. New TaQL functions in eval_math_func
echo "--- TaQL evaluator extensions ---"
SRC="$ROOT/src/taql.cpp"
for fn in LTRIM RTRIM CAPITALIZE SREVERSE SUBSTR REPLACE ISCOLUMN ISKEYWORD ISNULL ISDEF RAND ROWNR; do
    grep -q "\"$fn\"" "$SRC" && pass "func: $fn" || fail "func: $fn missing"
done

# 2. Regex evaluator
grep -q 'regex_expr' "$SRC" && pass "eval: regex_expr handler" || fail "eval: regex_expr missing"

# 3. LIKE improvement
grep -q 'std::regex_match' "$SRC" && pass "eval: LIKE via regex" || fail "eval: LIKE regex missing"

# 4. near 3-arg
grep -q 'args.size() == 3' "$SRC" && pass "eval: near 3-arg" || fail "eval: near 3-arg missing"

# 5. Test files
echo "--- Test files ---"
[[ -f "$ROOT/tests/taql_w7_closure_test.cpp" ]] && pass "W7 closure test exists" || fail "W7 closure test missing"
grep -q 'taql_w7_closure_test' "$ROOT/CMakeLists.txt" && pass "W7 test in CMakeLists" || fail "W7 test not in CMakeLists"

# 6. Checklists updated
echo "--- Checklists ---"
TAQL_CSV="$ROOT/docs/phase11/taql_command_checklist.csv"
MSSEL_CSV="$ROOT/docs/phase11/msselection_capability_checklist.csv"
TAQL_DONE=$(grep -c ',Done,' "$TAQL_CSV" || true)
[[ "$TAQL_DONE" -gt 0 ]] && pass "TaQL checklist has $TAQL_DONE Done rows" || fail "TaQL checklist no Done rows"
MSSEL_DONE=$(grep -c ',Done,' "$MSSEL_CSV" || true)
[[ "$MSSEL_DONE" -gt 0 ]] && pass "MSSel checklist has $MSSEL_DONE Done rows" || fail "MSSel checklist no Done rows"
# No Pending rows should remain in either checklist
if grep -q ',Pending,' "$TAQL_CSV"; then
    fail "TaQL checklist has Pending rows"
else
    pass "TaQL checklist: no Pending rows"
fi
if grep -q ',Pending,' "$MSSEL_CSV"; then
    fail "MSSel checklist has Pending rows"
else
    pass "MSSel checklist: no Pending rows"
fi

# 7. Build and run all tests
echo "--- Build & Test ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
    cmake --build "$BUILD" --target taql_w7_closure_test taql_eval_test taql_command_test ms_selection_table_expr_bridge_test ms_selection_parser_test ms_selection_malformed_test ms_selection_test ms_selection_parity_test 2>&1 | tail -2
    "$BUILD/taql_w7_closure_test" 2>&1 | grep -q '0 failed' && pass "taql_w7_closure_test passes" || fail "taql_w7_closure_test fails"
    "$BUILD/taql_eval_test" 2>&1 | grep -q '0 failed' && pass "taql_eval_test passes" || fail "taql_eval_test fails"
    "$BUILD/taql_command_test" 2>&1 | grep -q '0 failed' && pass "taql_command_test passes" || fail "taql_command_test fails"
    "$BUILD/ms_selection_table_expr_bridge_test" 2>&1 | grep -q '0 failed' && pass "bridge_test passes" || fail "bridge_test fails"
    "$BUILD/ms_selection_parser_test" 2>&1 | grep -q '0 failed' && pass "parser_test passes" || fail "parser_test fails"
    "$BUILD/ms_selection_malformed_test" 2>&1 | grep -q '0 failed' && pass "malformed_test passes" || fail "malformed_test fails"
    "$BUILD/ms_selection_test" 2>&1 | grep -q 'all.*passed' && pass "ms_selection_test passes" || fail "ms_selection_test regression"
    "$BUILD/ms_selection_parity_test" 2>&1 | grep -q 'all.*passed' && pass "parity_test passes" || fail "parity_test regression"
else
    fail "build directory not found"
fi

echo ""
echo "=== P11-W7 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
