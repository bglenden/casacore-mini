#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 full acceptance suite"

bash tools/check_phase7_w3.sh "${BUILD_DIR}"
bash tools/check_phase7_w4.sh "${BUILD_DIR}"
bash tools/check_phase7_w5.sh "${BUILD_DIR}"
bash tools/check_phase7_w6.sh "${BUILD_DIR}"
bash tools/check_phase7_w7.sh "${BUILD_DIR}"
bash tools/check_phase7_w8.sh "${BUILD_DIR}"
# W9 is skipped here because W9 calls check_phase7.sh itself (meta-gate).
bash tools/check_phase7_w11.sh "${BUILD_DIR}"
bash tools/check_phase7_w12.sh "${BUILD_DIR}"
bash tools/check_phase7_w13.sh "${BUILD_DIR}"
bash tools/check_phase7_w14.sh "${BUILD_DIR}"
bash tools/check_phase7_w15.sh "${BUILD_DIR}"
bash tools/check_phase7_w16.sh "${BUILD_DIR}"
bash tools/check_phase7_w17.sh "${BUILD_DIR}"
bash tools/check_phase7_w18.sh "${BUILD_DIR}"

# Run the full interop matrix when casacore is available.
# W17 hardening enforces this as a gating check — any matrix failure fails the
# full acceptance suite.
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists casacore; then
  echo "casacore detected — running full interop matrix (GATING)"
  bash tools/interop/run_phase7_matrix.sh "${BUILD_DIR}"
  echo "Interop matrix: ALL PASS"
else
  echo "casacore not detected — skipping interop matrix"
fi

echo "Phase 7 full acceptance suite passed"
