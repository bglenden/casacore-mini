#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P8-W9 gate: Coordinate utility layer + persistence integration
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W9 gate ==="

for h in include/casacore_mini/coordinate_util.hpp \
         include/casacore_mini/fits_coordinate_util.hpp \
         include/casacore_mini/gaussian_convert.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for s in src/coordinate_util.cpp \
         src/fits_coordinate_util.cpp \
         src/gaussian_convert.cpp; do
  [ -f "$s" ] && echo "  PASS  $s exists" || { echo "  FAIL  $s missing"; exit 1; }
done

for t in coordinate_util_test fits_coordinate_util_test gaussian_convert_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W9 gate PASSED ==="
