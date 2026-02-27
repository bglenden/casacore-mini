#!/usr/bin/env bash
# P9-W13 oracle conformance gate.
# Verifies casacore-mini reads the upstream test MS cell-by-cell
# against a committed oracle dump.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"
TGZFILE_DEFAULT="${ROOT_DIR}/casacore-original/ms/MSSel/test/mssel_test_small_multifield_spw.ms.tgz"
TGZFILE_LEGACY="/Users/brianglendenning/SoftwareProjects/casacore/main/ms/MSSel/test/mssel_test_small_multifield_spw.ms.tgz"
TGZFILE="${CASACORE_MINI_ORACLE_TGZ:-${TGZFILE_DEFAULT}}"
ORACLE_FILE="${ROOT_DIR}/data/corpus/oracle/mssel_multifield_spw_oracle.txt"

cd "${ROOT_DIR}"

echo "=== P9-W13: Oracle conformance gate ==="

if [[ ! -f "${ORACLE_FILE}" ]]; then
  echo "ERROR: oracle dump not found: ${ORACLE_FILE}"
  echo "Run 'bash tools/interop/generate_oracle.sh ${BUILD_DIR}' first."
  exit 1
fi

if [[ ! -f "${TGZFILE}" ]]; then
  if [[ -f "${TGZFILE_LEGACY}" ]]; then
    TGZFILE="${TGZFILE_LEGACY}"
  else
    echo "ERROR: test MS tarball not found: ${TGZFILE}"
    exit 1
  fi
fi

MINI_TOOL="${BUILD_DIR}/interop_mini_tool"
if [[ ! -x "${MINI_TOOL}" ]]; then
  echo "Building interop_mini_tool..."
  cmake --build "${BUILD_DIR}" --target interop_mini_tool -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
fi

# Extract MS to a temp directory.
TMPDIR_MS="$(mktemp -d)"
trap 'rm -rf "${TMPDIR_MS}"' EXIT

echo "Extracting test MS to ${TMPDIR_MS}..."
tar xzf "${TGZFILE}" -C "${TMPDIR_MS}"

# Find the extracted MS directory.
MS_DIR="$(find "${TMPDIR_MS}" -name 'table.dat' -maxdepth 2 -print -quit | xargs dirname)"
if [[ -z "${MS_DIR}" ]]; then
  echo "ERROR: could not find extracted MS directory"
  exit 1
fi

echo "MS directory: ${MS_DIR}"
echo "Oracle file:  ${ORACLE_FILE}"

"${MINI_TOOL}" verify-oracle-ms --input "${MS_DIR}" --oracle "${ORACLE_FILE}"

echo "=== P9-W13: Oracle conformance gate PASSED ==="
