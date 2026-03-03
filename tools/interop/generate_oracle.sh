#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
# Generate the oracle dump from the upstream casacore test MS.
# Requires: casacore_interop_tool built with casacore support.
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"
TGZFILE="${ROOT_DIR}/casacore-original/ms/MSSel/test/mssel_test_small_multifield_spw.ms.tgz"
ORACLE_DIR="${ROOT_DIR}/data/corpus/oracle"
ORACLE_FILE="${ORACLE_DIR}/mssel_multifield_spw_oracle.txt"

cd "${ROOT_DIR}"

if [[ ! -f "${TGZFILE}" ]]; then
  echo "ERROR: test MS tarball not found: ${TGZFILE}"
  exit 1
fi

TOOL="${BUILD_DIR}/casacore_interop_tool"
if [[ ! -x "${TOOL}" ]]; then
  echo "Building casacore_interop_tool..."
  cmake --build "${BUILD_DIR}" --target casacore_interop_tool -j "$(sysctl -n hw.ncpu 2>/dev/null || nproc)"
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

# Generate oracle dump.
mkdir -p "${ORACLE_DIR}"
"${TOOL}" dump-oracle-ms --input "${MS_DIR}" --output "${ORACLE_FILE}"

echo "Oracle dump written to: ${ORACLE_FILE}"
echo "Lines: $(wc -l < "${ORACLE_FILE}")"
echo "Size:  $(du -h "${ORACLE_FILE}" | cut -f1)"
