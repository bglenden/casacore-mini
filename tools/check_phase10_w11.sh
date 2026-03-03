#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 10 W11 gate: hardening + corpus expansion + docs convergence"

FAIL=0

# 1. Hardening test binaries must exist and pass.
for t in image_malformed_test lel_malformed_test; do
    BIN="${BUILD_DIR}/${t}"
    if [[ ! -x "${BIN}" ]]; then
        echo "FAIL: ${t} binary not found at ${BIN}"
        FAIL=1
        continue
    fi
    if ! "${BIN}" 2>&1; then
        echo "FAIL: ${t} returned nonzero"
        FAIL=1
    else
        echo "PASS: ${t}"
    fi
done

# 2. api_surface_map.csv must have no pending entries.
CSV="docs/phase10/api_surface_map.csv"
if [[ ! -f "${CSV}" ]]; then
    echo "FAIL: ${CSV} not found"
    FAIL=1
else
    PENDING=$(grep -c ',pending,' "${CSV}" || true)
    if [[ "${PENDING}" -gt 0 ]]; then
        echo "FAIL: ${CSV} has ${PENDING} pending entries"
        FAIL=1
    else
        echo "PASS: api_surface_map.csv — all entries done"
    fi
fi

# 3. LEL parse doc must list value() and mask() extractors.
LEL_HDR="include/casacore_mini/lattice_expr.hpp"
if ! grep -q 'value(expr)' "${LEL_HDR}"; then
    echo "FAIL: lel_parse Doxygen missing value(expr)"
    FAIL=1
fi
if ! grep -q 'mask(expr)' "${LEL_HDR}"; then
    echo "FAIL: lel_parse Doxygen missing mask(expr)"
    FAIL=1
fi
echo "PASS: LEL Doxygen completeness"

# 4. ImageConcat Doxygen must not claim lazy.
IMG_HDR="include/casacore_mini/image.hpp"
if grep -q 'lazily read' "${IMG_HDR}"; then
    echo "FAIL: ImageConcat Doxygen still claims lazy read"
    FAIL=1
else
    echo "PASS: ImageConcat Doxygen accuracy"
fi

if [[ "${FAIL}" -ne 0 ]]; then
    echo "P10-W11 gate: FAILED"
    exit 1
fi

echo "P10-W11 gate: ALL PASSED"
