#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-prepush}"
DOC_BUILD_DIR="${2:-build-prepush-doc}"

cd "${ROOT_DIR}"

bash tools/check_format.sh
bash tools/check_ci_build_lint_test_coverage.sh "${BUILD_DIR}"
bash tools/check_docs.sh "${DOC_BUILD_DIR}"

echo "pre-push quality checks passed"
