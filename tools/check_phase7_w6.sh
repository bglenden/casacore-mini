#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-ci-local}"

echo "Phase 7 W6: Running table_directory_test..."
"${BUILD_DIR}/table_directory_test"
echo "Phase 7 W6 checks passed"
