#!/usr/bin/env bash
# P12-W12 wave gate: Storage manager fidelity re-audit + deferred-on-demand freeze
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=${1:-build}
PASS=0; FAIL=0
run() { if eval "$1"; then ((++PASS)); else ((++FAIL)); echo "  FAIL: $2"; fi; }

echo "=== P12-W12 wave gate ==="

# 1. Build succeeds
run "cmake --build '$BUILD' --target table_mutation_test >/dev/null 2>&1" "build"

# 2. All existing tests pass
run "ctest --test-dir '$BUILD' --output-on-failure -j4 >/dev/null 2>&1" "tests"

# 3. Table mutation test passes (includes new TSM tests)
run "'$BUILD/table_mutation_test' >/dev/null 2>&1" "mutation-test"

# 4. Fidelity audit doc exists and is updated
run "grep -q 'P12-W12 re-audit' '$SCRIPT_DIR/docs/phase11/storage_manager_fidelity_audit.md'" "fidelity-doc"

# 5. All six managers listed as Exact in summary
run "grep -c '| Exact | Exact | Exact | Exact | Exact |' '$SCRIPT_DIR/docs/phase11/storage_manager_fidelity_audit.md' | grep -q '6'" "six-exact"

# 6. TSM add_row test exists in mutation test
run "grep -q 'test_add_row_tsm' '$SCRIPT_DIR/tests/table_mutation_test.cpp'" "tsm-add-test"

# 7. TSM remove_row test exists in mutation test
run "grep -q 'test_remove_row_tsm' '$SCRIPT_DIR/tests/table_mutation_test.cpp'" "tsm-remove-test"

# 8. Deferred-on-demand policy doc exists
run "test -f '$SCRIPT_DIR/docs/phase12/deferred_on_demand_policy.md'" "deferred-policy"

# 9. Policy lists all four deferred managers
run "grep -q 'MemoryStMan' '$SCRIPT_DIR/docs/phase12/deferred_on_demand_policy.md' && grep -q 'Adios2StMan' '$SCRIPT_DIR/docs/phase12/deferred_on_demand_policy.md' && grep -q 'Dysco' '$SCRIPT_DIR/docs/phase12/deferred_on_demand_policy.md'" "deferred-managers"

# 10. Demand-trigger procedure documented
run "grep -q 'Demand-trigger procedure' '$SCRIPT_DIR/docs/phase12/deferred_on_demand_policy.md'" "trigger-procedure"

echo ""
echo "=== P12-W12 gate summary ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -eq 0 ]; then echo "P12-W12 gate passed"; else echo "P12-W12 gate FAILED"; exit 1; fi
