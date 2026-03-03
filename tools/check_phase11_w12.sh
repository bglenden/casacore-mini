#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Gate script for Phase 11, Wave 12: Final closeout and maintenance handoff
set -euo pipefail
PASS=0; FAIL=0; TOTAL=0
ROOT="$(cd "$(dirname "$0")/.." && pwd)"

pass() { ((PASS++)); ((TOTAL++)); echo "  PASS: $1"; }
fail() { ((FAIL++)); ((TOTAL++)); echo "  FAIL: $1"; }

echo "=== P11-W12 Gate: Final closeout ==="

# 1. Exit report exists
echo "--- Closeout documents ---"
[[ -f "$ROOT/docs/phase11/exit_report.md" ]] && pass "exit_report.md" || fail "exit_report.md missing"
[[ -f "$ROOT/docs/phase11/known_differences.md" ]] && pass "known_differences.md" || fail "known_differences.md missing"

# 2. Checklists have zero Pending
echo "--- Checklist closure ---"
TAQL_CSV="$ROOT/docs/phase11/taql_command_checklist.csv"
MSSEL_CSV="$ROOT/docs/phase11/msselection_capability_checklist.csv"
if grep -q ',Pending,' "$TAQL_CSV"; then
    fail "TaQL checklist has Pending rows"
else
    pass "TaQL checklist: no Pending"
fi
if grep -q ',Pending,' "$MSSEL_CSV"; then
    fail "MSSel checklist has Pending rows"
else
    pass "MSSel checklist: no Pending"
fi

# 3. All wave gate scripts pass
echo "--- Wave gates ---"
for w in 1 2 3 4 5 6 7 8 9 10 11; do
    script="$ROOT/tools/check_phase11_w${w}.sh"
    if [[ -f "$script" ]]; then
        if bash "$script" > /dev/null 2>&1; then
            pass "W${w} gate passes"
        else
            fail "W${w} gate fails"
        fi
    else
        fail "W${w} gate script missing"
    fi
done

# 4. Review packets exist for all waves
echo "--- Review packets ---"
for w in W1 W2 W3 W4 W5 W6 W7 W8 W9 W10 W11 W12; do
    [[ -d "$ROOT/docs/phase11/review/P11-$w" ]] && pass "P11-$w review dir" || fail "P11-$w review dir missing"
done

echo ""
echo "=== P11-W12 Gate: $PASS/$TOTAL passed, $FAIL failed ==="
[[ $FAIL -eq 0 ]] && exit 0 || exit 1
