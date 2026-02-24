#!/usr/bin/env bash
# P8-W8 gate: CoordinateSystem composition
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W8 gate ==="

for h in include/casacore_mini/coordinate_system.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for s in src/coordinate_system.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

for t in coordinate_system_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W8 gate PASSED ==="
