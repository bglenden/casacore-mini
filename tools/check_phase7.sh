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

echo "Phase 7 full acceptance suite passed"
