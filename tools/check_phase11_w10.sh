#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Gate script for Phase 11, Wave 10: Full-project interoperability closure matrix
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W10 Gate: Full-project interoperability closure ==="

# 1. Run all Phase 11 test suites (mini->mini matrix)
echo "--- Full Phase 11 test suite ---"
BUILD="$ROOT/build"
if [[ -d "$BUILD" ]]; then
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
else
    fail "build directory not found"
fi

# 2. Prior-phase matrix scripts exist
echo "--- Prior-phase matrix scripts ---"
for script in tools/interop/run_phase7_matrix.sh tools/interop/run_phase9_matrix.sh tools/interop/run_phase10_matrix.sh; do
    [[ -f "$ROOT/$script" ]] && pass "$script exists" || fail "$script missing"
done

# 3. Known differences documented
[[ -f "$ROOT/docs/phase11/known_differences.md" ]] && pass "known_differences.md exists" || fail "known_differences.md missing"

echo ""
echo "=== P11-W10 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
