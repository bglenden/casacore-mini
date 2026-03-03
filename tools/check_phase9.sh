#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 9 full acceptance suite"

bash tools/check_phase9_w1.sh "${BUILD_DIR}"
bash tools/check_phase9_w2.sh "${BUILD_DIR}"
bash tools/check_phase9_w3.sh "${BUILD_DIR}"
bash tools/check_phase9_w4.sh "${BUILD_DIR}"
bash tools/check_phase9_w5.sh "${BUILD_DIR}"
bash tools/check_phase9_w6.sh "${BUILD_DIR}"
bash tools/check_phase9_w7.sh "${BUILD_DIR}"
bash tools/check_phase9_w8.sh "${BUILD_DIR}"
bash tools/check_phase9_w9.sh "${BUILD_DIR}"
bash tools/check_phase9_w10.sh "${BUILD_DIR}"
bash tools/check_phase9_w11.sh "${BUILD_DIR}"
bash tools/check_phase9_w12.sh "${BUILD_DIR}"
bash tools/check_phase9_w13.sh "${BUILD_DIR}"

# Run the full interop matrix when casacore is available.
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists casacore; then
  echo "casacore detected — running Phase 9 interop matrix (GATING)"
  bash tools/interop/run_phase9_matrix.sh "${BUILD_DIR}"
else
  echo "casacore not detected — skipping Phase 9 interop matrix"
fi

bash tools/check_phase9_w14.sh "${BUILD_DIR}"

echo "Phase 9 full acceptance suite passed"
