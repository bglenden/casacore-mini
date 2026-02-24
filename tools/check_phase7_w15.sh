#!/usr/bin/env bash
# Phase 7 W15 gate: TiledColumnStMan + TiledCellStMan read/write
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

SRC=src/tiled_stman.cpp
HDR=include/casacore_mini/tiled_stman.hpp

echo "=== Phase 7 W15: TiledColumnStMan + TiledCellStMan Read/Write ==="

# TiledStManReader and TiledStManWriter classes exist in the header
check "TiledStManReader class declared"  grep -q "class TiledStManReader" "$HDR"
check "TiledStManWriter class declared"  grep -q "class TiledStManWriter" "$HDR"

# Key methods implemented in tiled_stman.cpp
check "TiledStManReader::open implemented"       grep -q "TiledStManReader::open"       "$SRC"
check "TiledStManReader::read_float_cell implemented" grep -q "TiledStManReader::read_float_cell" "$SRC"
check "TiledStManWriter::setup implemented"      grep -q "TiledStManWriter::setup"      "$SRC"
check "TiledStManWriter::write_files implemented" grep -q "TiledStManWriter::write_files" "$SRC"

# interop_mini_tool has tiled col/cell write and verify subcommands
check "interop_mini_tool has TiledColumnStMan write" grep -q "write-tiled-col-dir"  src/interop_mini_tool.cpp
check "interop_mini_tool has TiledCellStMan write"   grep -q "write-tiled-cell-dir" src/interop_mini_tool.cpp
check "interop_mini_tool has TiledColumnStMan verify" grep -q "verify-tiled-col-dir"  src/interop_mini_tool.cpp
check "interop_mini_tool has TiledCellStMan verify"   grep -q "verify-tiled-cell-dir" src/interop_mini_tool.cpp

# interop_mini_tool uses TiledStManWriter
check "interop_mini_tool uses TiledStManWriter" grep -q "TiledStManWriter" src/interop_mini_tool.cpp
check "interop_mini_tool uses TiledStManReader" grep -q "TiledStManReader" src/interop_mini_tool.cpp

# CMakeLists includes tiled_stman.cpp
check "CMakeLists includes tiled_stman.cpp" grep -q "tiled_stman.cpp" CMakeLists.txt

# Build and run mini round-trips
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"
if [ -d "$BUILD_DIR" ]; then
  check "TSM builds cleanly" cmake --build "$BUILD_DIR" --target interop_mini_tool
  TMPRT=$(mktemp -d)
  trap 'rm -rf "$TMPRT"' EXIT
  check "TiledCol mini write"   "$BUILD_DIR/interop_mini_tool" write-tiled-col-dir  --output "$TMPRT/tiled_col_rt"
  check "TiledCol mini verify"  "$BUILD_DIR/interop_mini_tool" verify-tiled-col-dir --input  "$TMPRT/tiled_col_rt"
  check "TiledCell mini write"  "$BUILD_DIR/interop_mini_tool" write-tiled-cell-dir --output "$TMPRT/tiled_cell_rt"
  check "TiledCell mini verify" "$BUILD_DIR/interop_mini_tool" verify-tiled-cell-dir --input "$TMPRT/tiled_cell_rt"
fi

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
