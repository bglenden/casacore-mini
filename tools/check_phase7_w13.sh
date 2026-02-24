#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 W13 gate: StandardStMan write path"

# 1. Verify SsmWriter is declared in standard_stman.hpp.
if ! grep -q "class SsmWriter" include/casacore_mini/standard_stman.hpp; then
  echo "FAIL: SsmWriter class not found in standard_stman.hpp" >&2
  exit 1
fi

# 2. Verify SsmWriter is implemented in standard_stman.cpp.
if ! grep -q "SsmWriter::setup" src/standard_stman.cpp; then
  echo "FAIL: SsmWriter::setup not found in standard_stman.cpp" >&2
  exit 1
fi

if ! grep -q "SsmWriter::write_cell" src/standard_stman.cpp; then
  echo "FAIL: SsmWriter::write_cell not found in standard_stman.cpp" >&2
  exit 1
fi

if ! grep -q "SsmWriter::flush" src/standard_stman.cpp; then
  echo "FAIL: SsmWriter::flush not found in standard_stman.cpp" >&2
  exit 1
fi

if ! grep -q "SsmWriter::make_blob" src/standard_stman.cpp; then
  echo "FAIL: SsmWriter::make_blob not found in standard_stman.cpp" >&2
  exit 1
fi

# 3. Verify interop_mini_tool write-table-dir uses SsmWriter.
if ! grep -q "SsmWriter" src/interop_mini_tool.cpp; then
  echo "FAIL: interop_mini_tool.cpp does not use SsmWriter" >&2
  exit 1
fi

# 4. Verify table_desc_writer.cpp writes SM data blobs.
if ! grep -q "data_blob" src/table_desc_writer.cpp; then
  echo "FAIL: table_desc_writer.cpp does not write SM data blobs" >&2
  exit 1
fi

# 5. Round-trip test: mini writes, mini verifies.
TMPDIR_W13="${BUILD_DIR}/phase7_w13_test"
rm -rf "${TMPDIR_W13}"
mkdir -p "${TMPDIR_W13}"

MINI_TOOL="${BUILD_DIR}/interop_mini_tool"
if [ -x "${MINI_TOOL}" ]; then
  if ! "${MINI_TOOL}" write-table-dir --output "${TMPDIR_W13}/ssm_table"; then
    echo "FAIL: mini write-table-dir failed" >&2
    exit 1
  fi

  if [ ! -f "${TMPDIR_W13}/ssm_table/table.f0" ]; then
    echo "FAIL: table.f0 not produced by mini write-table-dir" >&2
    exit 1
  fi

  if ! "${MINI_TOOL}" verify-table-dir --input "${TMPDIR_W13}/ssm_table"; then
    echo "FAIL: mini verify-table-dir failed on mini-produced table" >&2
    exit 1
  fi
else
  echo "WARN: interop_mini_tool not found in ${BUILD_DIR}" >&2
fi

echo "Phase 7 W13 gate passed"
