#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 full acceptance suite"

bash tools/check_phase10_w1.sh "${BUILD_DIR}"
bash tools/check_phase10_w2.sh "${BUILD_DIR}"
bash tools/check_phase10_w3.sh "${BUILD_DIR}"
bash tools/check_phase10_w4.sh "${BUILD_DIR}"
bash tools/check_phase10_w5.sh "${BUILD_DIR}"
bash tools/check_phase10_w6.sh "${BUILD_DIR}"
bash tools/check_phase10_w7.sh "${BUILD_DIR}"
bash tools/check_phase10_w8.sh "${BUILD_DIR}"
bash tools/check_phase10_w9.sh "${BUILD_DIR}"
bash tools/check_phase10_w10.sh "${BUILD_DIR}"
bash tools/check_phase10_w11.sh "${BUILD_DIR}"
bash tools/check_phase10_w12.sh "${BUILD_DIR}"

echo "Phase 10 full acceptance suite passed"
