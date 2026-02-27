#!/usr/bin/env bash
# Gate script for Phase 11, Wave 5: MS selection grammar/parser parity
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W5 Gate: MS selection grammar/parser parity ==="

# 1. Header has all 12 category setters + clear/reset
echo "--- API surface ---"
HDR="$ROOT/include/casacore_mini/ms_selection.hpp"
for fn in set_antenna_expr set_field_expr set_spw_expr set_scan_expr set_time_expr set_uvdist_expr set_corr_expr set_state_expr set_observation_expr set_array_expr set_feed_expr set_taql_expr; do
    grep -q "$fn" "$HDR" && pass "API: $fn" || fail "API: $fn missing"
done
grep -q 'void clear()' "$HDR" && pass "API: clear" || fail "API: clear missing"
grep -q 'void reset()' "$HDR" && pass "API: reset" || fail "API: reset missing"

# 2. Result struct has new fields
echo "--- Result struct ---"
grep -q 'observations' "$HDR" && pass "result: observations" || fail "result: observations missing"
grep -q 'arrays' "$HDR" && pass "result: arrays" || fail "result: arrays missing"
grep -q 'feeds' "$HDR" && pass "result: feeds" || fail "result: feeds missing"

# 3. Source has category implementations
echo "--- Category parsers ---"
SRC="$ROOT/src/ms_selection.cpp"
grep -q 'observation_expr_' "$SRC" && pass "impl: observation parser" || fail "impl: observation parser missing"
grep -q 'array_expr_' "$SRC" && pass "impl: array parser" || fail "impl: array parser missing"
grep -q 'feed_expr_' "$SRC" && pass "impl: feed parser" || fail "impl: feed parser missing"
grep -q 'taql_expr_' "$SRC" && pass "impl: TaQL injection" || fail "impl: TaQL injection missing"
grep -q 'taql_execute' "$SRC" && pass "impl: TaQL bridge call" || fail "impl: TaQL bridge missing"

# 4. Scan bounds support
grep -q "Scan: invalid bound" "$SRC" && pass "impl: scan bounds" || fail "impl: scan bounds missing"

# 5. Feed pair patterns
for pat in "auto_only" "with_auto" "feed_ids"; do
    grep -q "$pat" "$SRC" && pass "impl: feed $pat" || fail "impl: feed $pat missing"
done

# 6. Test files exist and registered
echo "--- Test files ---"
[[ -f "$ROOT/tests/ms_selection_parser_test.cpp" ]] && pass "ms_selection_parser_test.cpp exists" || fail "missing"
[[ -f "$ROOT/tests/ms_selection_malformed_test.cpp" ]] && pass "ms_selection_malformed_test.cpp exists" || fail "missing"
grep -q 'ms_selection_parser_test' "$ROOT/CMakeLists.txt" && pass "parser test in CMakeLists" || fail "not in CMakeLists"
grep -q 'ms_selection_malformed_test' "$ROOT/CMakeLists.txt" && pass "malformed test in CMakeLists" || fail "not in CMakeLists"

# 7. Build and run tests
echo "--- Build & Test ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
    cmake --build "$BUILD" --target ms_selection_parser_test ms_selection_malformed_test ms_selection_test ms_selection_parity_test 2>&1 | tail -2
    "$BUILD/ms_selection_parser_test" 2>&1 | grep -q '0 failed' && pass "parser_test passes" || fail "parser_test fails"
    "$BUILD/ms_selection_malformed_test" 2>&1 | grep -q '0 failed' && pass "malformed_test passes" || fail "malformed_test fails"
    "$BUILD/ms_selection_test" 2>&1 | grep -q 'all.*passed' && pass "existing ms_selection_test passes" || fail "regression in ms_selection_test"
    "$BUILD/ms_selection_parity_test" 2>&1 | grep -q 'all.*passed' && pass "existing parity_test passes" || fail "regression in parity_test"
else
    fail "build directory not found"
fi

# 8. Prerequisites
echo "--- Prerequisites ---"
grep -q 'TaqlLexer' "$ROOT/src/taql.cpp" && pass "W3/W4: TaQL intact" || fail "W3/W4: TaQL broken"
grep -q 'eval_expr' "$ROOT/src/taql.cpp" && pass "W4: evaluator intact" || fail "W4: evaluator broken"

echo ""
echo "=== P11-W5 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
