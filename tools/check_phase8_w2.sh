#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P8-W2 gate: Measure core model + serialization
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W2 gate ==="

# Required headers exist.
for h in include/casacore_mini/quantity.hpp \
         include/casacore_mini/measure_types.hpp \
         include/casacore_mini/measure_record.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

# Required sources exist.
for s in src/quantity.cpp src/measure_types.cpp src/measure_record.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

# Tests must pass.
for t in quantity_test measure_types_test measure_record_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W2 gate PASSED ==="
