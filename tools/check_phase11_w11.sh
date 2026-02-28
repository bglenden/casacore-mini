#!/usr/bin/env bash
# Gate script for Phase 11, Wave 11: Hardening + stress + docs convergence
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W11 Gate: Hardening + stress + docs ==="

# 1. Malformed input tests
echo "--- Malformed/hardening tests ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
    "$BUILD/ms_selection_malformed_test" 2>&1 | grep -q '0 failed' && pass "ms_selection malformed" || fail "malformed"
else
    fail "build directory not found"
fi

# 2. Known differences documented
echo "--- Documentation ---"
[[ -f "$ROOT/docs/phase11/known_differences.md" ]] && pass "known_differences.md" || fail "known_differences missing"
DIFF_COUNT=$(grep -c '^### D' "$ROOT/docs/phase11/known_differences.md" || true)
[[ "$DIFF_COUNT" -ge 10 ]] && pass "known_differences has $DIFF_COUNT items" || fail "too few known differences"

# 3. Doxygen annotations present for key APIs
echo "--- API documentation ---"
HDR_TAQL="$ROOT/include/casacore_mini/taql.hpp"
HDR_MSSEL="$ROOT/include/casacore_mini/ms_selection.hpp"
grep -q '/// ' "$HDR_TAQL" && pass "taql.hpp has Doxygen" || fail "taql.hpp missing docs"
grep -q '/// ' "$HDR_MSSEL" && pass "ms_selection.hpp has Doxygen" || fail "ms_selection.hpp missing docs"

# 4. All test suites pass (stress via full suite)
echo "--- Full suite regression ---"
for test in taql_eval_test taql_command_test taql_w7_closure_test \
            ms_selection_test ms_selection_parity_test ms_selection_parser_test \
            ms_selection_malformed_test ms_selection_table_expr_bridge_test \
            phase11_integration_test; do
    out=$("$BUILD/$test" 2>&1)
    if echo "$out" | grep -qE '0 failed|all.*passed'; then
        pass "$test"
    else
        fail "$test"
    fi
done

echo ""
echo "=== P11-W11 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
