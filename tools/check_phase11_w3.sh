#!/usr/bin/env bash
# Gate script for Phase 11, Wave 3: TaQL parser + AST foundation
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W3 Gate: TaQL parser + AST foundation ==="

# 1. Header exists and has key types
echo "--- Artifact checks ---"
HDR="$ROOT/include/casacore_mini/taql.hpp"
[[ -f "$HDR" ]] && pass "taql.hpp exists" || fail "taql.hpp missing"
grep -q 'TaqlCommand' "$HDR" && pass "TaqlCommand enum in header" || fail "TaqlCommand enum missing"
grep -q 'ExprType' "$HDR" && pass "ExprType enum in header" || fail "ExprType enum missing"
grep -q 'TaqlOp' "$HDR" && pass "TaqlOp enum in header" || fail "TaqlOp enum missing"
grep -q 'TaqlExprNode' "$HDR" && pass "TaqlExprNode struct in header" || fail "TaqlExprNode missing"
grep -q 'TaqlAst' "$HDR" && pass "TaqlAst struct in header" || fail "TaqlAst missing"
grep -q 'taql_parse' "$HDR" && pass "taql_parse declared" || fail "taql_parse missing"
grep -q 'taql_show' "$HDR" && pass "taql_show declared" || fail "taql_show missing"
grep -q 'taql_execute' "$HDR" && pass "taql_execute declared" || fail "taql_execute missing"
grep -q 'taql_calc' "$HDR" && pass "taql_calc declared" || fail "taql_calc missing"
grep -q 'TaqlValue' "$HDR" && pass "TaqlValue type in header" || fail "TaqlValue missing"

# 2. Source file exists and has parser class
SRC="$ROOT/src/taql.cpp"
[[ -f "$SRC" ]] && pass "taql.cpp exists" || fail "taql.cpp missing"
grep -q 'TaqlLexer' "$SRC" && pass "TaqlLexer class" || fail "TaqlLexer missing"
grep -q 'TaqlParser' "$SRC" && pass "TaqlParser class" || fail "TaqlParser missing"
grep -q 'parse_select' "$SRC" && pass "parse_select method" || fail "parse_select missing"
grep -q 'parse_update' "$SRC" && pass "parse_update method" || fail "parse_update missing"
grep -q 'parse_insert' "$SRC" && pass "parse_insert method" || fail "parse_insert missing"
grep -q 'parse_delete' "$SRC" && pass "parse_delete method" || fail "parse_delete missing"
grep -q 'parse_calc' "$SRC" && pass "parse_calc method" || fail "parse_calc missing"
grep -q 'parse_create_table' "$SRC" && pass "parse_create_table method" || fail "parse_create_table missing"
grep -q 'parse_alter_table' "$SRC" && pass "parse_alter_table method" || fail "parse_alter_table missing"
grep -q 'parse_drop_table' "$SRC" && pass "parse_drop_table method" || fail "parse_drop_table missing"
grep -q 'parse_show' "$SRC" && pass "parse_show method" || fail "parse_show missing"
grep -q 'parse_count' "$SRC" && pass "parse_count method" || fail "parse_count missing"

# 3. Test files exist
T1="$ROOT/tests/taql_parser_test.cpp"
T2="$ROOT/tests/taql_malformed_test.cpp"
[[ -f "$T1" ]] && pass "taql_parser_test.cpp exists" || fail "taql_parser_test.cpp missing"
[[ -f "$T2" ]] && pass "taql_malformed_test.cpp exists" || fail "taql_malformed_test.cpp missing"

# 4. Source is in CMakeLists
grep -q 'src/taql.cpp' "$ROOT/CMakeLists.txt" && pass "taql.cpp in CMakeLists" || fail "taql.cpp not in CMakeLists"
grep -q 'taql_parser_test' "$ROOT/CMakeLists.txt" && pass "taql_parser_test in CMakeLists" || fail "taql_parser_test not in CMakeLists"
grep -q 'taql_malformed_test' "$ROOT/CMakeLists.txt" && pass "taql_malformed_test in CMakeLists" || fail "taql_malformed_test not in CMakeLists"

# 5. All 10 command families covered
echo "--- Command families ---"
for CMD in select_cmd update_cmd insert_cmd delete_cmd count_cmd calc_cmd create_table_cmd alter_table_cmd drop_table_cmd show_cmd; do
    grep -q "$CMD" "$HDR" && pass "$CMD in TaqlCommand" || fail "$CMD missing from TaqlCommand"
done

# 6. Expression types coverage
echo "--- Expression types ---"
for ETYPE in literal column_ref keyword_ref unary_op binary_op comparison logical_op func_call aggregate_call in_expr between_expr like_expr regex_expr exists_expr subscript set_expr range_expr unit_expr iif_expr wildcard subquery record_literal; do
    grep -q "$ETYPE" "$HDR" && pass "ExprType::$ETYPE" || fail "ExprType::$ETYPE missing"
done

# 7. W1 prerequisite
echo "--- Prerequisites ---"
[[ -f "$ROOT/tools/check_phase11_w1.sh" ]] && pass "W1 gate exists" || fail "W1 gate missing"
[[ -f "$ROOT/tools/check_phase11_w2.sh" ]] && pass "W2 gate exists" || fail "W2 gate missing"

echo ""
echo "=== P11-W3 Result: $PASS passed, $FAIL failed out of $TOTAL ==="

# Update status
cat > "$ROOT/docs/phase11/review/phase11_status.json" <<JSONEOF
{
  "phase": 11,
  "wave": "W3",
  "gate_pass": $PASS,
  "gate_fail": $FAIL,
  "gate_total": $TOTAL,
  "status": "$([ "$FAIL" -eq 0 ] && echo 'pass' || echo 'fail')"
}
JSONEOF

exit "$FAIL"
