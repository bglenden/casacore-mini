#!/usr/bin/env bash
# P12-W11 wave gate: Table infrastructure tranche B
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=${1:-build}
PASS=0; FAIL=0
run() { if eval "$1"; then ((++PASS)); else ((++FAIL)); echo "  FAIL: $2"; fi; }

echo "=== P12-W11 wave gate ==="

# 1. Build succeeds
run "cmake --build '$BUILD' --target ref_table_test concat_table_test table_iter_test columns_index_test >/dev/null 2>&1" "build"

# 2. All existing tests pass
run "ctest --test-dir '$BUILD' --output-on-failure -j4 >/dev/null 2>&1" "tests"

# 3. RefTable test passes
run "'$BUILD/ref_table_test' >/dev/null 2>&1" "ref-table-test"

# 4. ConcatTable test passes
run "'$BUILD/concat_table_test' >/dev/null 2>&1" "concat-table-test"

# 5. TableIterator test passes
run "'$BUILD/table_iter_test' >/dev/null 2>&1" "table-iter-test"

# 6. ColumnsIndex test passes
run "'$BUILD/columns_index_test' >/dev/null 2>&1" "columns-index-test"

# 7. RefTable header exists
run "grep -q 'class RefTable' '$SCRIPT_DIR/include/casacore_mini/ref_table.hpp'" "ref-table-header"

# 8. ConcatTable header exists
run "grep -q 'class ConcatTable' '$SCRIPT_DIR/include/casacore_mini/concat_table.hpp'" "concat-table-header"

# 9. TableIterator header exists
run "grep -q 'class TableIterator' '$SCRIPT_DIR/include/casacore_mini/table_iterator.hpp'" "table-iterator-header"

# 10. ColumnsIndex header exists
run "grep -q 'class ColumnsIndex' '$SCRIPT_DIR/include/casacore_mini/columns_index.hpp'" "columns-index-header"

echo ""
echo "=== P12-W11 gate summary ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -eq 0 ]; then echo "P12-W11 gate passed"; else echo "P12-W11 gate FAILED"; exit 1; fi
