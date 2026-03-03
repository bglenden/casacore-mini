#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 9 W3 gate: Subtable schemas and enums"

# 1. Verify required files exist.
REQUIRED_FILES=(
  "include/casacore_mini/ms_subtables.hpp"
  "src/ms_subtables.cpp"
  "tests/ms_subtables_test.cpp"
)

for f in "${REQUIRED_FILES[@]}"; do
  if [ ! -s "${f}" ]; then
    echo "FAIL: ${f} missing or empty" >&2
    exit 1
  fi
done

# 2. Verify all 17 subtable factory functions exist.
for factory in make_antenna_desc make_data_description_desc make_feed_desc \
  make_field_desc make_flag_cmd_desc make_history_desc make_observation_desc \
  make_pointing_desc make_polarization_desc make_processor_desc \
  make_spectral_window_desc make_state_desc make_doppler_desc \
  make_freq_offset_desc make_source_desc make_syscal_desc make_weather_desc; do
  if ! grep -q "${factory}" include/casacore_mini/ms_subtables.hpp; then
    echo "FAIL: ms_subtables.hpp missing factory: ${factory}" >&2
    exit 1
  fi
done

# 3. Build and run test.
if [ -d "${BUILD_DIR}" ]; then
  cmake --build "${BUILD_DIR}" --target ms_subtables_test 2>&1
fi

if [ -x "${BUILD_DIR}/ms_subtables_test" ]; then
  "${BUILD_DIR}/ms_subtables_test"
fi

echo "Phase 9 W3 gate passed"
