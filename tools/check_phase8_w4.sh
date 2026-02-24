#!/usr/bin/env bash
# P8-W4 gate: Remaining measure types + conversion machines
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W4 gate ==="

for h in include/casacore_mini/velocity_machine.hpp include/casacore_mini/uvw_machine.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for s in src/velocity_machine.cpp src/uvw_machine.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

for t in velocity_machine_test measure_convert_full_test uvw_machine_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W4 gate PASSED ==="
