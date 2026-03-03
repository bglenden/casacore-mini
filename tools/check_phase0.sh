#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"
MANIFEST="${ROOT_DIR}/docs/phase0/corpus_manifest.yaml"
CHECK_DIR="${ROOT_DIR}/${BUILD_DIR}/phase0"

cd "${ROOT_DIR}"

python3 tools/validate_corpus_manifest.py "${MANIFEST}" --verify-paths --casacore-build-root "${CASACORE_BUILD_ROOT:-}"

mkdir -p "${CHECK_DIR}/run1" "${CHECK_DIR}/run2"

python3 tools/oracle_dump.py \
  --manifest "${MANIFEST}" \
  --artifact-id replay_ttableaccess \
  --output-dir "${CHECK_DIR}/run1" \
  --strict-checksum

python3 tools/oracle_dump.py \
  --manifest "${MANIFEST}" \
  --artifact-id replay_ttableaccess \
  --output-dir "${CHECK_DIR}/run2" \
  --strict-checksum

python3 tools/oracle_compare.py \
  --left "${CHECK_DIR}/run1/replay_ttableaccess.oracle.json" \
  --right "${CHECK_DIR}/run2/replay_ttableaccess.oracle.json"

echo "Phase 0 manifest + oracle determinism checks passed"
