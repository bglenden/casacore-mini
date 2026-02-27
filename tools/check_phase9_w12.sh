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

# 2. Plan status may be In Progress (mid-phase) or Complete (post-W14 closeout).
grep -Eq "^Status: (In Progress|Complete)" docs/phase9/plan.md || {
    echo "FAIL: phase9/plan.md status is neither In Progress nor Complete"; exit 1
}
echo "  plan status: OK"

# 3. Rolling plan must reference Phase 9 and its completion state (complete or
# in-progress text accepted due W13/W14 extension).
grep -Eq "Phase 9 \((complete|in progress|pending|In Progress)" docs/casacore_plan.md || {
    echo "FAIL: casacore_plan.md does not contain Phase 9 status text"; exit 1
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

# 5. At W12 closeout (or later), only post-W12 waves may remain pending.
pending_lines="$(grep "| Pending |" docs/phase9/plan.md 2>/dev/null || true)"
pending_count="$(printf '%s\n' "${pending_lines}" | sed '/^$/d' | wc -l | tr -d ' ')"
if [ "${pending_count}" -eq 0 ]; then
    echo "  pending waves: none"
elif [ "${pending_count}" -eq 1 ] && printf '%s' "${pending_lines}" | grep -q "P9-W14"; then
    echo "  pending waves: P9-W14 (expected post-W12)"
else
    echo "FAIL: unexpected pending waves in plan:"
    printf '%s\n' "${pending_lines}"
    exit 1
fi

# 6. Full test suite passes.
ctest --test-dir "$BUILD" --output-on-failure 2>&1 | tail -5
echo "  test suite: OK"

echo "=== P9-W12 gate PASSED ==="
