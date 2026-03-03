#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 9 Wave 14 gate: Final closeout.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W14 gate ==="

for f in docs/phase9/exit_report.md \
         docs/phase9/known_differences.md; do
    [[ -s "$f" ]] || { echo "FAIL: missing or empty $f"; exit 1; }
done
echo "  closeout docs present: OK"

grep -q "^Status: Complete" docs/phase9/plan.md || {
    echo "FAIL: phase9/plan.md status not Complete"; exit 1
}
grep -q "| \`P9-W14\` | Done |" docs/phase9/plan.md || {
    echo "FAIL: phase9/plan.md does not mark P9-W14 Done"; exit 1
}
echo "  phase9 plan closeout status: OK"

pending_lines="$(grep "| Pending |" docs/phase9/plan.md 2>/dev/null || true)"
if [[ -n "$(printf '%s' "${pending_lines}" | sed '/^$/d')" ]]; then
    echo "FAIL: pending waves remain in phase9 plan:"
    printf '%s\n' "${pending_lines}"
    exit 1
fi
echo "  no pending waves: OK"

grep -Eq "Phase 9 \(complete " docs/casacore_plan.md || {
    echo "FAIL: casacore_plan.md does not mark Phase 9 complete"; exit 1
}
grep -q "^Status: Complete" docs/phase9/exit_report.md || {
    echo "FAIL: exit_report status not Complete"; exit 1
}
grep -q "20/20" docs/phase9/exit_report.md || {
    echo "FAIL: exit_report missing 20/20 interop result"; exit 1
}
echo "  rolling/exit report consistency: OK"

for wave in $(seq 1 14); do
    dir="docs/phase9/review/P9-W${wave}"
    [[ -d "$dir" ]] || { echo "FAIL: missing review dir $dir"; exit 1; }
    for artifact in summary.md files_changed.txt check_results.txt \
                    matrix_results.json open_issues.md decisions.md; do
        [[ -f "$dir/$artifact" ]] || {
            echo "FAIL: missing $dir/$artifact"; exit 1
        }
    done
done
echo "  review packets complete (W1-W14): OK"

ctest --test-dir "$BUILD" --output-on-failure 2>&1 | tail -5
echo "  test suite: OK"

echo "=== P9-W14 gate PASSED ==="
