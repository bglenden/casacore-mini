#!/usr/bin/env bash
# Phase 9 Wave 12 gate: Closeout and Phase-10 handoff.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W12 gate ==="

# 1. Required closeout docs exist and are non-empty.
for f in docs/phase9/exit_report.md \
         docs/phase9/known_differences.md; do
    [[ -s "$f" ]] || { echo "FAIL: missing or empty $f"; exit 1; }
done
echo "  closeout docs present: OK"

# 2. Plan status is Complete.
grep -q "^Status: Complete" docs/phase9/plan.md || {
    echo "FAIL: phase9/plan.md status not Complete"; exit 1
}
echo "  plan status: OK"

# 3. casacore_plan.md references Phase 9 as complete.
grep -q "Phase 9 (complete" docs/casacore_plan.md || {
    echo "FAIL: casacore_plan.md does not show Phase 9 complete"; exit 1
}
echo "  rolling plan status: OK"

# 4. All 12 review packets exist (6 files each).
for wave in $(seq 1 12); do
    dir="docs/phase9/review/P9-W${wave}"
    [[ -d "$dir" ]] || { echo "FAIL: missing review dir $dir"; exit 1; }
    for artifact in summary.md files_changed.txt check_results.txt \
                    matrix_results.json open_issues.md decisions.md; do
        [[ -f "$dir/$artifact" ]] || {
            echo "FAIL: missing $dir/$artifact"; exit 1
        }
    done
done
echo "  review packets complete: OK"

# 5. All wave statuses in plan are Done.
pending_count=$(grep -c "| Pending |" docs/phase9/plan.md 2>/dev/null || true)
if [ "${pending_count}" -gt 0 ]; then
    echo "FAIL: ${pending_count} waves still Pending in plan"; exit 1
fi
echo "  all waves Done: OK"

# 6. Full test suite passes.
ctest --test-dir "$BUILD" --output-on-failure 2>&1 | tail -5
echo "  test suite: OK"

echo "=== P9-W12 gate PASSED ==="
