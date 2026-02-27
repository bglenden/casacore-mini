#!/usr/bin/env bash
# Gate script for Phase 11, Wave 4: TaQL evaluator + command execution
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W4 Gate: TaQL evaluator + command execution ==="

# 1. Source contains evaluator functions
echo "--- Evaluator implementation ---"
SRC="$ROOT/src/taql.cpp"
grep -q 'eval_expr' "$SRC" && pass "eval_expr defined" || fail "eval_expr missing"
grep -q 'eval_math_func' "$SRC" && pass "eval_math_func defined" || fail "eval_math_func missing"
grep -q 'cell_to_taql' "$SRC" && pass "cell_to_taql defined" || fail "cell_to_taql missing"
grep -q 'as_double' "$SRC" && pass "as_double helper" || fail "as_double missing"
grep -q 'as_int' "$SRC" && pass "as_int helper" || fail "as_int missing"
grep -q 'as_bool' "$SRC" && pass "as_bool helper" || fail "as_bool missing"
grep -q 'as_string' "$SRC" && pass "as_string helper" || fail "as_string missing"
grep -q 'compare_values' "$SRC" && pass "compare_values helper" || fail "compare_values missing"

# 2. Command execution
echo "--- Command execution ---"
grep -q 'select_cmd' "$SRC" && pass "SELECT execution" || fail "SELECT missing"
grep -q 'update_cmd' "$SRC" && pass "UPDATE execution" || fail "UPDATE missing"
grep -q 'delete_cmd' "$SRC" && pass "DELETE execution" || fail "DELETE missing"
grep -q 'count_cmd' "$SRC" && pass "COUNT execution" || fail "COUNT missing"
grep -q 'calc_cmd' "$SRC" && pass "CALC execution" || fail "CALC missing"
grep -q 'show_cmd' "$SRC" && pass "SHOW execution" || fail "SHOW missing"

# 3. Math built-in functions
echo "--- Built-in functions ---"
for fn in SIN COS TAN ASIN ACOS ATAN EXP LOG SQRT ABS ROUND FLOOR CEIL SQUARE CUBE; do
    grep -q "\"$fn\"" "$SRC" && pass "func: $fn" || fail "func: $fn missing"
done
for fn in ATAN2 POW FMOD MIN MAX; do
    grep -q "\"$fn\"" "$SRC" && pass "func: $fn" || fail "func: $fn missing"
done
for fn in STRLEN UPCASE DOWNCASE TRIM; do
    grep -q "\"$fn\"" "$SRC" && pass "func: $fn" || fail "func: $fn missing"
done
grep -q '"PI"' "$SRC" && pass "constant: PI" || fail "constant: PI missing"
grep -q '"E"' "$SRC" && pass "constant: E" || fail "constant: E missing"
grep -q '"C"' "$SRC" && pass "constant: C" || fail "constant: C missing"

# 4. Expression types evaluated
echo "--- Expression types ---"
for etype in literal column_ref unary_op binary_op comparison logical_op func_call iif_expr in_expr between_expr like_expr set_expr wildcard keyword_ref; do
    grep -q "ExprType::$etype" "$SRC" && pass "eval: $etype" || fail "eval: $etype missing"
done

# 5. Test files exist and are registered
echo "--- Test files ---"
[[ -f "$ROOT/tests/taql_eval_test.cpp" ]] && pass "taql_eval_test.cpp exists" || fail "taql_eval_test.cpp missing"
[[ -f "$ROOT/tests/taql_command_test.cpp" ]] && pass "taql_command_test.cpp exists" || fail "taql_command_test.cpp missing"
grep -q 'taql_eval_test' "$ROOT/CMakeLists.txt" && pass "taql_eval_test in CMakeLists" || fail "taql_eval_test not in CMakeLists"
grep -q 'taql_command_test' "$ROOT/CMakeLists.txt" && pass "taql_command_test in CMakeLists" || fail "taql_command_test not in CMakeLists"

# 6. Build and run tests
echo "--- Build & Test ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
    cmake --build "$BUILD" --target taql_eval_test taql_command_test taql_parser_test taql_malformed_test 2>&1 | tail -2
    "$BUILD/taql_eval_test" 2>&1 | grep -q '0 failed' && pass "taql_eval_test passes" || fail "taql_eval_test fails"
    "$BUILD/taql_command_test" 2>&1 | grep -q '0 failed' && pass "taql_command_test passes" || fail "taql_command_test fails"
    "$BUILD/taql_parser_test" 2>&1 | grep -q '0 failed' && pass "taql_parser_test passes" || fail "taql_parser_test fails"
    "$BUILD/taql_malformed_test" 2>&1 | grep -q '0 failed' && pass "taql_malformed_test passes" || fail "taql_malformed_test fails"
else
    fail "build directory not found"
fi

# 7. Prerequisites from W3
echo "--- Prerequisites (W3) ---"
grep -q 'TaqlLexer' "$SRC" && pass "W3 lexer intact" || fail "W3 lexer broken"
grep -q 'TaqlParser' "$SRC" && pass "W3 parser intact" || fail "W3 parser broken"

echo ""
echo "=== P11-W4 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
