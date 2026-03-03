#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

BUILD_DIR="${1:-build-ci-local}"

echo "Phase 7 W5: Running keyword_record_convert_test..."
"${BUILD_DIR}/keyword_record_convert_test"

echo "Phase 7 W5 checks passed"
