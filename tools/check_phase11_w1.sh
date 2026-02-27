#!/usr/bin/env bash
# Phase 11 Wave 1 gate: remaining-capabilities audit + disposition freeze
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

pass=0
fail=0
evidence_paths=()

check() {
    local desc="$1"
    local path="$2"
    if [[ -f "$ROOT/$path" ]]; then
        echo "  PASS: $desc ($path)"
        ((pass++)) || true
        evidence_paths+=("$path")
    else
        echo "  FAIL: $desc ($path missing)"
        ((fail++)) || true
    fi
}

check_csv_columns() {
    local desc="$1"
    local path="$2"
    shift 2
    local required_cols=("$@")
    if [[ ! -f "$ROOT/$path" ]]; then
        echo "  FAIL: $desc ($path missing)"
        ((fail++)) || true
        return
    fi
    local header
    header=$(head -1 "$ROOT/$path")
    local missing=()
    for col in "${required_cols[@]}"; do
        if ! echo "$header" | grep -q "$col"; then
            missing+=("$col")
        fi
    done
    if [[ ${#missing[@]} -gt 0 ]]; then
        echo "  FAIL: $desc - missing columns: ${missing[*]}"
        ((fail++)) || true
    else
        echo "  PASS: $desc (all required columns present)"
        ((pass++)) || true
    fi
}

check_nonempty_csv() {
    local desc="$1"
    local path="$2"
    if [[ ! -f "$ROOT/$path" ]]; then
        echo "  FAIL: $desc ($path missing)"
        ((fail++)) || true
        return
    fi
    local lines
    lines=$(wc -l < "$ROOT/$path")
    if [[ "$lines" -lt 2 ]]; then
        echo "  FAIL: $desc (no data rows)"
        ((fail++)) || true
    else
        echo "  PASS: $desc ($((lines - 1)) data rows)"
        ((pass++)) || true
    fi
}

echo "=== Phase 11 Wave 1 Gate ==="
echo ""

echo "--- Audit artifacts ---"
check "missing_capabilities_audit.csv" "docs/phase11/missing_capabilities_audit.csv"
check "disposition_decisions.md" "docs/phase11/disposition_decisions.md"
check "api_surface_map.csv" "docs/phase11/api_surface_map.csv"
check "storage_manager_fidelity_audit.md" "docs/phase11/storage_manager_fidelity_audit.md"
check "taql_command_checklist.csv" "docs/phase11/taql_command_checklist.csv"
check "msselection_capability_checklist.csv" "docs/phase11/msselection_capability_checklist.csv"

echo ""
echo "--- Checklist schema validation ---"
check_csv_columns "taql checklist columns" "docs/phase11/taql_command_checklist.csv" \
    "family" "feature" "upstream_source" "mini_symbol" "status" "target_wave" \
    "test_id" "interop_artifact" "commit_sha" "pr_link" "evidence_path" "oracle_diff_path"

check_csv_columns "mssel checklist columns" "docs/phase11/msselection_capability_checklist.csv" \
    "family" "feature" "upstream_source" "mini_symbol" "status" "target_wave" \
    "test_id" "interop_artifact" "commit_sha" "pr_link" "evidence_path" "oracle_diff_path"

check_csv_columns "audit CSV columns" "docs/phase11/missing_capabilities_audit.csv" \
    "module" "capability" "upstream_source" "interop_impact" "workflow_impact" \
    "disposition" "target_wave" "simplification_status" "remediation_wave"

echo ""
echo "--- Checklist data content ---"
check_nonempty_csv "taql checklist data" "docs/phase11/taql_command_checklist.csv"
check_nonempty_csv "mssel checklist data" "docs/phase11/msselection_capability_checklist.csv"
check_nonempty_csv "audit data" "docs/phase11/missing_capabilities_audit.csv"
check_nonempty_csv "api surface map data" "docs/phase11/api_surface_map.csv"

echo ""
echo "--- Disposition coverage ---"
# Check that audit has both Include and Exclude rows
if [[ -f "$ROOT/docs/phase11/missing_capabilities_audit.csv" ]]; then
    include_count=$(grep -c ",Include," "$ROOT/docs/phase11/missing_capabilities_audit.csv" || true)
    exclude_count=$(grep -c ",Exclude," "$ROOT/docs/phase11/missing_capabilities_audit.csv" || true)
    if [[ "$include_count" -gt 0 && "$exclude_count" -gt 0 ]]; then
        echo "  PASS: audit has Include ($include_count) and Exclude ($exclude_count) rows"
        ((pass++)) || true
    else
        echo "  FAIL: audit must have both Include and Exclude rows (Include=$include_count, Exclude=$exclude_count)"
        ((fail++)) || true
    fi
fi

# Check TaQL checklist covers required command families
if [[ -f "$ROOT/docs/phase11/taql_command_checklist.csv" ]]; then
    required_families=("SELECT" "UPDATE" "INSERT" "DELETE" "COUNT" "CALC" "CREATE_TABLE" "ALTER_TABLE" "DROP_TABLE" "SHOW")
    missing_families=()
    for fam in "${required_families[@]}"; do
        if ! grep -q "^$fam," "$ROOT/docs/phase11/taql_command_checklist.csv"; then
            missing_families+=("$fam")
        fi
    done
    if [[ ${#missing_families[@]} -eq 0 ]]; then
        echo "  PASS: TaQL checklist covers all 10 command families"
        ((pass++)) || true
    else
        echo "  FAIL: TaQL checklist missing families: ${missing_families[*]}"
        ((fail++)) || true
    fi
fi

# Check MSSel checklist covers required categories
if [[ -f "$ROOT/docs/phase11/msselection_capability_checklist.csv" ]]; then
    required_cats=("ANTENNA" "FIELD" "SPW" "TIME" "SCAN" "ARRAY" "UVDIST" "CORR" "STATE" "OBSERVATION" "FEED" "TAQL")
    missing_cats=()
    for cat in "${required_cats[@]}"; do
        if ! grep -q "^$cat," "$ROOT/docs/phase11/msselection_capability_checklist.csv"; then
            missing_cats+=("$cat")
        fi
    done
    if [[ ${#missing_cats[@]} -eq 0 ]]; then
        echo "  PASS: MSSel checklist covers all 12 categories"
        ((pass++)) || true
    else
        echo "  FAIL: MSSel checklist missing categories: ${missing_cats[*]}"
        ((fail++)) || true
    fi
fi

echo ""
echo "--- Status file update ---"
# Write status JSON
mkdir -p "$ROOT/docs/phase11/review"
cat > "$ROOT/docs/phase11/review/phase11_status.json" <<STATUSEOF
{
  "phase": 11,
  "wave": 1,
  "timestamp_utc": "$(date -u +%Y-%m-%dT%H:%M:%SZ)",
  "gate_results": {
    "check_phase11_w1.sh": {
      "pass": $pass,
      "fail": $fail,
      "command": "bash tools/check_phase11_w1.sh"
    }
  },
  "checklist_totals": {
    "taql": {
      "Pending": $(grep -c ",Pending," "$ROOT/docs/phase11/taql_command_checklist.csv" 2>/dev/null || echo 0),
      "Done": $(grep -c ",Done," "$ROOT/docs/phase11/taql_command_checklist.csv" 2>/dev/null || echo 0)
    },
    "mssel": {
      "Pending": $(grep -c ",Pending," "$ROOT/docs/phase11/msselection_capability_checklist.csv" 2>/dev/null || echo 0),
      "Done": $(grep -c ",Done," "$ROOT/docs/phase11/msselection_capability_checklist.csv" 2>/dev/null || echo 0)
    }
  },
  "open_blockers": [],
  "new_evidence_paths": [$(printf '"%s",' "${evidence_paths[@]}" | sed 's/,$//')]
}
STATUSEOF
echo "  PASS: phase11_status.json updated"
((pass++)) || true

echo ""
echo "=== P11-W1 Result: pass=$pass fail=$fail ==="
if [[ "$fail" -gt 0 ]]; then
    exit 1
fi
echo "P11-W1 GATE PASSED"
