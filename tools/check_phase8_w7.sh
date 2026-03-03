#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P8-W7 gate: Concrete coordinate classes
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W7 gate ==="

for h in include/casacore_mini/direction_coordinate.hpp \
         include/casacore_mini/spectral_coordinate.hpp \
         include/casacore_mini/stokes_coordinate.hpp \
         include/casacore_mini/linear_coordinate.hpp \
         include/casacore_mini/tabular_coordinate.hpp \
         include/casacore_mini/quality_coordinate.hpp; do
  [ -f "$h" ] && echo "  PASS  $h exists" || { echo "  FAIL  $h missing"; exit 1; }
done

for t in direction_coordinate_test spectral_coordinate_test stokes_coordinate_test coordinate_record_test; do
  if ctest --test-dir "$BUILD_DIR" -R "^${t}$" --output-on-failure; then
    echo "  PASS  $t"
  else
    echo "  FAIL  $t"
    exit 1
  fi
done

echo "=== P8-W7 gate PASSED ==="
