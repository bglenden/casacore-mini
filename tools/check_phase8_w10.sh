#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# P8-W10 gate: Strict Phase 8 interop matrix (24 cells)
set -euo pipefail

BUILD_DIR="${1:-build}"

echo "=== P8-W10 gate ==="

# The interop matrix requires casacore to be installed.
if ! command -v pkg-config >/dev/null 2>&1 || ! pkg-config --exists casacore; then
  echo "  SKIP  casacore not detected — skipping Phase 8 interop matrix"
  echo "=== P8-W10 gate SKIPPED (no casacore) ==="
  exit 0
fi

# Run the full 24-cell matrix.
bash tools/interop/run_phase8_matrix.sh "${BUILD_DIR}"

echo "=== P8-W10 gate PASSED ==="
