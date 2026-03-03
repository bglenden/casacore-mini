#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 7 W18 gate: final closeout — wave gates + docs evidence
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

PASS=0; FAIL=0
check() {
  local desc="$1"; shift
  if "$@" >/dev/null 2>&1; then
    echo "  PASS  $desc"; PASS=$((PASS + 1))
  else
    echo "  FAIL  $desc"; FAIL=$((FAIL + 1))
  fi
}

echo "=== Phase 7 W18: Final Closeout ==="

# 1. All wave gates pass
echo "--- Wave gate chain ---"
check "W11 gate passes" bash tools/check_phase7_w11.sh "${BUILD_DIR}"
check "W12 gate passes" bash tools/check_phase7_w12.sh "${BUILD_DIR}"
check "W13 gate passes" bash tools/check_phase7_w13.sh "${BUILD_DIR}"
check "W14 gate passes" bash tools/check_phase7_w14.sh "${BUILD_DIR}"
check "W15 gate passes" bash tools/check_phase7_w15.sh "${BUILD_DIR}"
check "W16 gate passes" bash tools/check_phase7_w16.sh "${BUILD_DIR}"
check "W17 gate passes" bash tools/check_phase7_w17.sh "${BUILD_DIR}"

# 2. Exit report exists and contains key evidence markers
EXIT_REPORT="docs/phase7/exit_report.md"
echo "--- Exit report evidence ---"
check "Exit report exists"             test -f "${EXIT_REPORT}"
check "Exit report marks phase complete" grep -q "Phase 7 is \*\*complete\*\*" "${EXIT_REPORT}"
check "Exit report lists StandardStMan"    grep -q "StandardStMan"    "${EXIT_REPORT}"
check "Exit report lists IncrementalStMan" grep -q "IncrementalStMan" "${EXIT_REPORT}"
check "Exit report lists TiledColumnStMan" grep -q "TiledColumnStMan" "${EXIT_REPORT}"
check "Exit report lists TiledCellStMan"   grep -q "TiledCellStMan"   "${EXIT_REPORT}"
check "Exit report lists TiledShapeStMan"  grep -q "TiledShapeStMan"  "${EXIT_REPORT}"
check "Exit report lists TiledDataStMan"   grep -q "TiledDataStMan"   "${EXIT_REPORT}"
check "Exit report references 24 matrix cells" grep -q "24" "${EXIT_REPORT}"
check "Exit report references W18 gate pass"   grep -q "W18" "${EXIT_REPORT}"

# 3. casacore_plan.md marks Phase 7 as complete
ROLLING_PLAN="docs/casacore_plan.md"
echo "--- Rolling plan status ---"
check "casacore_plan.md marks Phase 7 complete" grep -q "Phase 7 (complete" "${ROLLING_PLAN}"

# 4. deferred_managers.md no longer claims managers are deferred
DEFERRED="docs/phase7/deferred_managers.md"
check "deferred_managers.md updated"               test -f "${DEFERRED}"
check "deferred_managers.md shows all implemented" grep -q "fully implemented" "${DEFERRED}"

# 5. check_phase7.sh wires all waves and has gating matrix
FULL_SUITE="tools/check_phase7.sh"
echo "--- Acceptance suite completeness ---"
check "check_phase7.sh references W15 gate" grep -q "check_phase7_w15.sh" "${FULL_SUITE}"
check "check_phase7.sh references W16 gate" grep -q "check_phase7_w16.sh" "${FULL_SUITE}"
check "check_phase7.sh references W17 gate" grep -q "check_phase7_w17.sh" "${FULL_SUITE}"
check "check_phase7.sh references W18 gate" grep -q "check_phase7_w18.sh" "${FULL_SUITE}"
check "check_phase7.sh has gating matrix"   grep -q "GATING" "${FULL_SUITE}"

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
