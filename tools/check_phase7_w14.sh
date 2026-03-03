#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Phase 7  W14 gate: ISM read/write
set -euo pipefail

PASS=0; FAIL=0
check() {
  local desc="$1"; shift
  if "$@" >/dev/null 2>&1; then
    echo "  PASS  $desc"; PASS=$((PASS + 1))
  else
    echo "  FAIL  $desc"; FAIL=$((FAIL + 1))
  fi
}

SRC=src/incremental_stman.cpp
HDR=include/casacore_mini/incremental_stman.hpp

echo "=== Phase 7  W14: ISM Read/Write ==="

# IsmReader and IsmWriter classes exist
check "IsmReader class declared"  grep -q "class IsmReader" "$HDR"
check "IsmWriter class declared"  grep -q "class IsmWriter" "$HDR"

# Implementation exists
check "IsmReader::open implemented"   grep -q "IsmReader::open" "$SRC"
check "IsmReader::read_cell implemented" grep -q "IsmReader::read_cell" "$SRC"
check "IsmWriter::flush implemented"  grep -q "IsmWriter::flush" "$SRC"
check "IsmWriter::write_file implemented" grep -q "IsmWriter::write_file" "$SRC"

# ISM fixture exists
check "ISM fixture table.dat exists" test -f tests/fixtures/ism_table/table.dat
check "ISM fixture table.f0 exists"  test -f tests/fixtures/ism_table/table.f0

# interop_mini_tool has ISM write and verify
check "interop_mini_tool has IsmWriter usage" grep -q "IsmWriter" src/interop_mini_tool.cpp
check "interop_mini_tool has IsmReader usage" grep -q "IsmReader" src/interop_mini_tool.cpp

# CMakeLists includes incremental_stman.cpp
check "CMakeLists includes incremental_stman.cpp" grep -q "incremental_stman.cpp" CMakeLists.txt

# Build and run mini round-trip
BUILD_DIR="${1:-build-ci-local}"
if [ -d "$BUILD_DIR" ]; then
  check "ISM builds cleanly" cmake --build "$BUILD_DIR" --target interop_mini_tool
  TMPRT=$(mktemp -d)
  trap 'rm -rf "$TMPRT"' EXIT
  check "ISM mini write" "$BUILD_DIR/interop_mini_tool" write-ism-dir --output "$TMPRT/ism_rt"
  check "ISM mini verify (self)" "$BUILD_DIR/interop_mini_tool" verify-ism-dir --input "$TMPRT/ism_rt"
  check "ISM mini verify (casacore fixture)" "$BUILD_DIR/interop_mini_tool" verify-ism-dir --input tests/fixtures/ism_table --relaxed-keywords
fi

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
