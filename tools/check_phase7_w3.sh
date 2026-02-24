#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

# Phase 7 Wave 3: Active binary metadata integration.
# Validates that read_table_binary_metadata produces parity with text path.

if [ ! -f "${BUILD_DIR}/table_binary_schema_test" ]; then
  echo "FAIL: ${BUILD_DIR}/table_binary_schema_test not found (build first)" >&2
  exit 1
fi

"${BUILD_DIR}/table_binary_schema_test"

echo "Phase 7 W3 checks passed"
