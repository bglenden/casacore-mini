#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"

cd "${ROOT_DIR}"

# Phase 5 write-path metadata compatibility checks.
# Validates that:
#   1. AipsIoWriter round-trips through AipsIoReader (tested by aipsio_writer_test).
#   2. Record binary serialization round-trip still holds (tested by record_io_test).
#   3. AipsIO Record reader decodes casacore wire format (tested by aipsio_record_reader_test).
#   4. Fidelity contract v1 document exists.

if [ ! -f "docs/phase5/keyword_record_fidelity_v1.md" ]; then
  echo "FAIL: docs/phase5/keyword_record_fidelity_v1.md not found" >&2
  exit 1
fi

if [ ! -f "${BUILD_DIR}/aipsio_writer_test" ]; then
  echo "FAIL: ${BUILD_DIR}/aipsio_writer_test not found (build first)" >&2
  exit 1
fi

"${BUILD_DIR}/aipsio_writer_test"

if [ ! -f "${BUILD_DIR}/record_io_test" ]; then
  echo "FAIL: ${BUILD_DIR}/record_io_test not found (build first)" >&2
  exit 1
fi

"${BUILD_DIR}/record_io_test"

if [ ! -f "${BUILD_DIR}/aipsio_record_reader_test" ]; then
  echo "FAIL: ${BUILD_DIR}/aipsio_record_reader_test not found (build first)" >&2
  exit 1
fi

"${BUILD_DIR}/aipsio_record_reader_test"

echo "Phase 5 checks passed"
