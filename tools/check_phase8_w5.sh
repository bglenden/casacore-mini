#!/usr/bin/env bash
# P8-W5 gate: TableMeasures descriptors + column integration
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W5 gate ==="

for h in include/casacore_mini/table_measure_desc.hpp include/casacore_mini/table_measure_column.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for s in src/table_measure_desc.cpp src/table_measure_column.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

for t in table_measure_desc_test table_measure_column_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W5 gate PASSED ==="
