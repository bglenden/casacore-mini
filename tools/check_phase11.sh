#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 11 aggregate gate: runs all wave checks
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${1:-build-p11-dbg}"

pass=0
fail=0

run_wave() {
    local wave="$1"
    local script="$SCRIPT_DIR/check_phase11_w${wave}.sh"
    if [[ -x "$script" ]]; then
        echo "=== Running W${wave} gate ==="
        if bash "$script"; then
            echo "  W${wave}: PASSED"
            ((pass++)) || true
        else
            echo "  W${wave}: FAILED"
            ((fail++)) || true
        fi
        echo ""
    fi
}

echo "============================================"
echo "  Phase 11 Aggregate Gate"
echo "============================================"
echo ""

# Run all existing wave checks
for w in 1 2 3 4 5 6 7 8 9 10 11 12; do
    run_wave "$w"
done

# Run CI build/lint/test
echo "=== Running CI build/lint/test gate ==="
if bash "$SCRIPT_DIR/check_ci_build_lint_test_coverage.sh" "$BUILD_DIR" 2>&1 | tail -5; then
    echo "  CI gate: PASSED"
    ((pass++)) || true
else
    echo "  CI gate: FAILED"
    ((fail++)) || true
fi

echo ""
echo "============================================"
echo "  Phase 11 Aggregate: pass=$pass fail=$fail"
echo "============================================"

if [[ "$fail" -gt 0 ]]; then
    exit 1
fi
echo "PHASE 11 AGGREGATE GATE PASSED"
