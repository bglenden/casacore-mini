#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

BUILD_DIR="${1:-build-ci-local}"

echo "Phase 7 W6: Running table_directory_test..."
"${BUILD_DIR}/table_directory_test"
echo "Phase 7 W6 checks passed"
