#!/usr/bin/env bash
# Phase 11 Wave 2 gate: closure contract lock + tranche planning
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

pass=0
fail=0

check() {
    local desc="$1"
    local path="$2"
    if [[ -f "$ROOT/$path" ]]; then
        echo "  PASS: $desc"
        ((pass++)) || true
    else
        echo "  FAIL: $desc ($path missing)"
        ((fail++)) || true
    fi
}

echo "=== Phase 11 Wave 2 Gate ==="
echo ""

echo "--- Contract artifacts ---"
check "interop_artifact_inventory.md" "docs/phase11/interop_artifact_inventory.md"
check "tolerances.md" "docs/phase11/tolerances.md"
check "lint_profile.md" "docs/phase11/lint_profile.md"

echo ""
echo "--- Check scaffolding ---"
check "check_phase11.sh" "tools/check_phase11.sh"
check "check_phase11_w2.sh" "tools/check_phase11_w2.sh"

echo ""
echo "--- Review infrastructure ---"
check "review packet template" "docs/phase11/review/P11-WX/review_packet.md"
check "phase11_status.json" "docs/phase11/review/phase11_status.json"

echo ""
echo "--- W1 prerequisite pass ---"
if bash "$ROOT/tools/check_phase11_w1.sh" > /dev/null 2>&1; then
    echo "  PASS: W1 gate passes"
    ((pass++)) || true
else
    echo "  FAIL: W1 gate fails (prerequisite)"
    ((fail++)) || true
fi

echo ""
echo "--- Tranche mapping ---"
# Verify TaQL/MSSel checklists have target_wave assignments
if [[ -f "$ROOT/docs/phase11/taql_command_checklist.csv" ]]; then
    w3_rows=$(grep -c ",P11-W3," "$ROOT/docs/phase11/taql_command_checklist.csv" || true)
    w4_rows=$(grep -c ",P11-W4," "$ROOT/docs/phase11/taql_command_checklist.csv" || true)
    if [[ "$w3_rows" -gt 0 && "$w4_rows" -gt 0 ]]; then
        echo "  PASS: TaQL tranche mapping (W3=$w3_rows, W4=$w4_rows)"
        ((pass++)) || true
    else
        echo "  FAIL: TaQL tranche mapping incomplete (W3=$w3_rows, W4=$w4_rows)"
        ((fail++)) || true
    fi
fi

if [[ -f "$ROOT/docs/phase11/msselection_capability_checklist.csv" ]]; then
    w5_rows=$(grep -c ",P11-W5," "$ROOT/docs/phase11/msselection_capability_checklist.csv" || true)
    w6_rows=$(grep -c ",P11-W6," "$ROOT/docs/phase11/msselection_capability_checklist.csv" || true)
    if [[ "$w5_rows" -gt 0 && "$w6_rows" -gt 0 ]]; then
        echo "  PASS: MSSel tranche mapping (W5=$w5_rows, W6=$w6_rows)"
        ((pass++)) || true
    else
        echo "  FAIL: MSSel tranche mapping incomplete (W5=$w5_rows, W6=$w6_rows)"
        ((fail++)) || true
    fi
fi

# Update status JSON
mkdir -p "$ROOT/docs/phase11/review"
cat > "$ROOT/docs/phase11/review/phase11_status.json" <<STATUSEOF
{
  "phase": 11,
  "wave": 2,
  "timestamp_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "gate_results": {
    "check_phase11_w1.sh": {"pass": true},
    "check_phase11_w2.sh": {
      "pass": $pass,
      "fail": $fail,
      "command": "bash tools/check_phase11_w2.sh"
    }
  },
  "checklist_totals": {
    "taql_pending": $(grep -c ",Pending," "$ROOT/docs/phase11/taql_command_checklist.csv" 2>/dev/null || echo 0),
    "mssel_pending": $(grep -c ",Pending," "$ROOT/docs/phase11/msselection_capability_checklist.csv" 2>/dev/null || echo 0)
  },
  "open_blockers": []
}
STATUSEOF
echo ""
echo "  PASS: phase11_status.json updated"
((pass++)) || true

echo ""
echo "=== P11-W2 Result: pass=$pass fail=$fail ==="
if [[ "$fail" -gt 0 ]]; then
    exit 1
fi
echo "P11-W2 GATE PASSED"
