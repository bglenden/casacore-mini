#!/usr/bin/env bash
# P12-W13 wave gate: mdspan foundation + migration
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD=${1:-build}
PASS=0; FAIL=0
run() { if eval "$1"; then ((++PASS)); else ((++FAIL)); echo "  FAIL: $2"; fi; }

echo "=== P12-W13 wave gate ==="

# 1. Build succeeds
run "cmake --build '$BUILD' --target mdspan_migration_test >/dev/null 2>&1" "build"

# 2. All existing tests pass
run "ctest --test-dir '$BUILD' --output-on-failure -j4 >/dev/null 2>&1" "tests"

# 3. mdspan migration test passes
run "'$BUILD/mdspan_migration_test' >/dev/null 2>&1" "mdspan-test"

# 4. mdspan_compat.hpp exists with shim
run "grep -q 'mdspan_compat' '$SCRIPT_DIR/include/casacore_mini/mdspan_compat.hpp'" "mdspan-compat-header"

# 5. strided_fortran_copy helper exists
run "grep -q 'strided_fortran_copy' '$SCRIPT_DIR/include/casacore_mini/lattice_shape.hpp'" "strided-copy-helper"

# 6. strided_fortran_scatter helper exists
run "grep -q 'strided_fortran_scatter' '$SCRIPT_DIR/include/casacore_mini/lattice_shape.hpp'" "strided-scatter-helper"

# 7. make_const_lattice_span helper exists
run "grep -q 'make_const_lattice_span' '$SCRIPT_DIR/include/casacore_mini/lattice_shape.hpp'" "make-const-span"

# 8. make_lattice_span helper exists
run "grep -q 'make_lattice_span' '$SCRIPT_DIR/include/casacore_mini/lattice_shape.hpp'" "make-span"

# 9. LatticeArray get_slice uses strided_fortran_copy
run "grep -q 'strided_fortran_copy' '$SCRIPT_DIR/include/casacore_mini/lattice_array.hpp'" "get-slice-migrated"

# 10. LatticeArray put_slice uses strided_fortran_scatter
run "grep -q 'strided_fortran_scatter' '$SCRIPT_DIR/include/casacore_mini/lattice_array.hpp'" "put-slice-migrated"

echo ""
echo "=== P12-W13 gate summary ==="
echo "PASS: $PASS"
echo "FAIL: $FAIL"
if [ "$FAIL" -eq 0 ]; then echo "P12-W13 gate passed"; else echo "P12-W13 gate FAILED"; exit 1; fi
