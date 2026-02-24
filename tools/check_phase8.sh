#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 8 full acceptance suite"

bash tools/check_phase8_w1.sh "${BUILD_DIR}"
bash tools/check_phase8_w2.sh "${BUILD_DIR}"
bash tools/check_phase8_w3.sh "${BUILD_DIR}"
bash tools/check_phase8_w4.sh "${BUILD_DIR}"
bash tools/check_phase8_w5.sh "${BUILD_DIR}"
bash tools/check_phase8_w6.sh "${BUILD_DIR}"
bash tools/check_phase8_w7.sh "${BUILD_DIR}"
bash tools/check_phase8_w8.sh "${BUILD_DIR}"
bash tools/check_phase8_w9.sh "${BUILD_DIR}"
bash tools/check_phase8_w10.sh "${BUILD_DIR}"
bash tools/check_phase8_w11.sh "${BUILD_DIR}"
bash tools/check_phase8_w12.sh "${BUILD_DIR}"

# Run the full interop matrix when casacore is available.
if command -v pkg-config >/dev/null 2>&1 && pkg-config --exists casacore; then
  echo "casacore detected — running Phase 8 interop matrix (GATING)"
  bash tools/interop/run_phase8_matrix.sh "${BUILD_DIR}"
  echo "Phase 8 interop matrix: ALL PASS"
else
  echo "casacore not detected — skipping Phase 8 interop matrix"
fi

echo "Phase 8 full acceptance suite passed"
