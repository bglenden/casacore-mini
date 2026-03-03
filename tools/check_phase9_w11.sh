#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 9 Wave 11 gate: Interop matrix + hardening.
set -euo pipefail

BUILD="${1:?usage: $0 <build-dir>}"

echo "=== P9-W11 gate ==="

# 1. Required source files exist.
for f in tests/ms_malformed_test.cpp \
         tools/interop/run_phase9_matrix.sh; do
    [[ -f "$f" ]] || { echo "FAIL: missing $f"; exit 1; }
done
echo "  source files present: OK"

# 2. interop_mini_tool has MS produce/verify commands.
for cmd in produce-ms-minimal verify-ms-minimal \
           produce-ms-representative verify-ms-representative \
           produce-ms-optional-subtables verify-ms-optional-subtables \
           produce-ms-concat verify-ms-concat \
           produce-ms-selection-stress verify-ms-selection-stress; do
    grep -q "\"${cmd}\"" src/interop_mini_tool.cpp || {
        echo "FAIL: interop_mini_tool missing command ${cmd}"; exit 1
    }
done
echo "  interop commands in mini tool: OK"

# 3. casacore_interop_tool has MS produce/verify commands.
for cmd in produce-ms-minimal verify-ms-minimal \
           produce-ms-representative verify-ms-representative \
           produce-ms-optional-subtables verify-ms-optional-subtables \
           produce-ms-concat verify-ms-concat \
           produce-ms-selection-stress verify-ms-selection-stress; do
    grep -q "\"${cmd}\"" tools/interop/casacore_interop_tool.cpp || {
        echo "FAIL: casacore_interop_tool missing command ${cmd}"; exit 1
    }
done
echo "  interop commands in casacore tool: OK"

# 4. Build succeeds.
cmake --build "$BUILD" --target ms_malformed_test --target interop_mini_tool 2>&1 | tail -1
echo "  build: OK"

# 5. Hardening tests pass.
ctest --test-dir "$BUILD" -R "ms_malformed_test" --output-on-failure
echo "  hardening tests: OK"

# 6. Mini-tool self-interop (mini→mini round-trip for all 5 artifacts).
MINI="${BUILD}/interop_mini_tool"
TMPDIR_ART=$(mktemp -d)
trap 'rm -rf "${TMPDIR_ART}"' EXIT

for art in ms-minimal ms-representative ms-optional-subtables ms-concat ms-selection-stress; do
    "${MINI}" "produce-${art}" --output "${TMPDIR_ART}/${art}" 2>/dev/null
    "${MINI}" "verify-${art}" --input "${TMPDIR_ART}/${art}" 2>/dev/null
done
echo "  mini→mini round-trip: OK"

echo "=== P9-W11 gate PASSED ==="
