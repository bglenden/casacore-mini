#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="${1:-build-ci-local}"

echo "Phase 7 W5: Running keyword_record_convert_test..."
"${BUILD_DIR}/keyword_record_convert_test"

echo "Phase 7 W5 checks passed"
