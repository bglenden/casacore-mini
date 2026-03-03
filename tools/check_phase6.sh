#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"

cd "${ROOT_DIR}"

# Phase 6 binary record interoperability and table write bootstrap checks.

if [ ! -f "data/corpus/fixtures/phase6_binary_record_provenance.md" ]; then
  echo "FAIL: data/corpus/fixtures/phase6_binary_record_provenance.md not found" >&2
  exit 1
fi

for fixture in \
  "data/corpus/fixtures/logtable_stdstman_keywords/column_TIME_keywords.bin" \
  "data/corpus/fixtures/ms_tree/table_keywords.bin" \
  "data/corpus/fixtures/ms_tree/column_UVW_keywords.bin" \
  "data/corpus/fixtures/pagedimage_coords/table_keywords.bin"; do
  if [ ! -f "${fixture}" ]; then
    echo "FAIL: ${fixture} not found" >&2
    exit 1
  fi
done

for test_bin in aipsio_record_writer_test record_metadata_test table_dat_writer_test; do
  if [ ! -f "${BUILD_DIR}/${test_bin}" ]; then
    echo "FAIL: ${BUILD_DIR}/${test_bin} not found (build first)" >&2
    exit 1
  fi
  "${BUILD_DIR}/${test_bin}"
done

echo "Phase 6 checks passed"
