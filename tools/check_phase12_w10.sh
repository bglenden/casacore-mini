#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P12-W10 wave gate: Table infrastructure tranche A
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=${1:-build}
PASS=0; FAIL=0
run() { if eval "$1"; then ((++PASS)); else ((++FAIL)); echo "  FAIL: $2"; fi; }
SRC="$SCRIPT_DIR/src/table.cpp"
HDR="$SCRIPT_DIR/include/casacore_mini/table.hpp"

echo "=== P12-W10 wave gate ==="

# 1. Build succeeds
run "cmake --build '$BUILD' --target table_lock_test table_info_test >/dev/null 2>&1" "build"

# 2. All existing tests pass
run "ctest --test-dir '$BUILD' --output-on-failure -j4 >/dev/null 2>&1" "tests"

# 3. Lock test passes
run "'$BUILD/table_lock_test' >/dev/null 2>&1" "lock-test"

# 4. Info test passes
run "'$BUILD/table_info_test' >/dev/null 2>&1" "info-test"

# 5. TableLockMode enum in header
run "grep -q 'enum class TableLockMode' '$HDR'" "lock-mode-enum"

# 6. TableInfo struct in header
run "grep -q 'struct TableInfo' '$HDR'" "table-info-struct"

# 7. parse_data_type_name utility
run "grep -q 'parse_data_type_name' '$HDR'" "data-type-parser"

# 8. drop_table method
run "grep -q 'drop_table' '$HDR'" "drop-table"

# 9. private_keywords method
run "grep -q 'private_keywords' '$HDR'" "private-keywords"

# 10. TaQL uses parse_data_type_name (no more duplicated parse_dt lambda)
run "! grep -q 'auto parse_dt' '$SCRIPT_DIR/src/taql.cpp'" "taql-dedup"

echo ""
echo "=== P12-W10 gate summary ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -eq 0 ]; then echo "P12-W10 gate passed"; else echo "P12-W10 gate FAILED"; exit 1; fi
