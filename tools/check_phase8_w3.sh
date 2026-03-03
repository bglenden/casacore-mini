#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P8-W3 gate: Primary measure types (MEpoch, MPosition, MDirection) + ERFA
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W3 gate ==="

for h in include/casacore_mini/erfa_bridge.hpp \
         include/casacore_mini/measure_convert.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for s in src/erfa_bridge.cpp src/measure_convert.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

for t in erfa_bridge_test measure_convert_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W3 gate PASSED ==="
