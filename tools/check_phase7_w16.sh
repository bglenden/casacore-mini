#!/usr/bin/env bash
# Phase 7 W16 gate: TiledShapeStMan + TiledDataStMan read/write
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

echo "=== Phase 7 W16: TiledShapeStMan + TiledDataStMan Read/Write ==="

# TiledStManReader and TiledStManWriter classes exist (shared with W15)
check "TiledStManReader class declared"  grep -q "class TiledStManReader" "$HDR"
check "TiledStManWriter class declared"  grep -q "class TiledStManWriter" "$HDR"

# Key methods implemented
check "TiledStManWriter::setup implemented"       grep -q "TiledStManWriter::setup"       "$SRC"
check "TiledStManWriter::write_files implemented" grep -q "TiledStManWriter::write_files"  "$SRC"
check "TiledStManReader::open implemented"        grep -q "TiledStManReader::open"         "$SRC"
check "TiledStManReader::read_float_cell implemented" grep -q "TiledStManReader::read_float_cell" "$SRC"

# interop_mini_tool has tiled shape/data write and verify subcommands
check "interop_mini_tool has TiledShapeStMan write"  grep -q "write-tiled-shape-dir" src/interop_mini_tool.cpp
check "interop_mini_tool has TiledDataStMan write"   grep -q "write-tiled-data-dir"  src/interop_mini_tool.cpp
check "interop_mini_tool has TiledShapeStMan verify" grep -q "verify-tiled-shape-dir" src/interop_mini_tool.cpp
check "interop_mini_tool has TiledDataStMan verify"  grep -q "verify-tiled-data-dir"  src/interop_mini_tool.cpp

# CMakeLists includes tiled_stman.cpp
check "CMakeLists includes tiled_stman.cpp" grep -q "tiled_stman.cpp" CMakeLists.txt

# Build and run mini round-trips
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"
if [ -d "$BUILD_DIR" ]; then
  check "TSM builds cleanly" cmake --build "$BUILD_DIR" --target interop_mini_tool
  TMPRT=$(mktemp -d)
  trap 'rm -rf "$TMPRT"' EXIT
  check "TiledShape mini write"   "$BUILD_DIR/interop_mini_tool" write-tiled-shape-dir  --output "$TMPRT/tiled_shape_rt"
  check "TiledShape mini verify"  "$BUILD_DIR/interop_mini_tool" verify-tiled-shape-dir --input  "$TMPRT/tiled_shape_rt"
  check "TiledData mini write"    "$BUILD_DIR/interop_mini_tool" write-tiled-data-dir   --output "$TMPRT/tiled_data_rt"
  check "TiledData mini verify"   "$BUILD_DIR/interop_mini_tool" verify-tiled-data-dir  --input  "$TMPRT/tiled_data_rt"
fi

echo
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ]
