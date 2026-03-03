#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 W8 gate: full directory matrix expansion"

# Verify all directory-level tests pass.
"${BUILD_DIR}/table_directory_test"
"${BUILD_DIR}/tiled_directory_test"

# Verify the matrix runner includes all six required manager cases.
MATRIX_SCRIPT="tools/interop/run_phase7_matrix.sh"
for case_name in table_dir ism_dir tiled_col_dir tiled_cell_dir tiled_shape_dir tiled_data_dir; do
  if ! grep -q "\"${case_name}\"" "${MATRIX_SCRIPT}"; then
    echo "FAIL: matrix runner missing case '${case_name}'" >&2
    exit 1
  fi
done

echo "Phase 7 W8 gate passed"
