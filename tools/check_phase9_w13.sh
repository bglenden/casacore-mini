#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 9 Wave 13 gate: Oracle conformance.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W13 gate ==="

for f in tools/check_oracle.sh \
         docs/phase9/oracle_conformance_plan.md \
         data/corpus/oracle/mssel_multifield_spw_oracle.txt; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  oracle inputs/scripts present: OK"

grep -q "\"verify-oracle-ms\"" src/interop_mini_tool.cpp || {
    echo "FAIL: interop_mini_tool missing verify-oracle-ms command"; exit 1
}
echo "  verify-oracle-ms command present: OK"

TGZ_PRIMARY="${ROOT_DIR}/casacore-original/ms/MSSel/test/mssel_test_small_multifield_spw.ms.tgz"
TGZ_LEGACY="/Users/brianglendenning/SoftwareProjects/casacore/main/ms/MSSel/test/mssel_test_small_multifield_spw.ms.tgz"
if [[ -n "${CASACORE_MINI_ORACLE_TGZ:-}" || -f "${TGZ_PRIMARY}" || -f "${TGZ_LEGACY}" ]]; then
    bash tools/check_oracle.sh "${BUILD}"
    echo "  oracle gate run: OK"
else
    echo "  oracle test MS not found; skipping P9-W13 runtime check"
fi

echo "=== P9-W13 gate PASSED ==="
