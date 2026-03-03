#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P8-W6 gate: Coordinate primitives + projection foundation
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W6 gate ==="

for h in include/casacore_mini/coordinate.hpp include/casacore_mini/projection.hpp \
         include/casacore_mini/linear_xform.hpp include/casacore_mini/obs_info.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for s in src/coordinate.cpp src/projection.cpp src/linear_xform.cpp src/obs_info.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

for t in projection_test linear_xform_test obs_info_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W6 gate PASSED ==="
