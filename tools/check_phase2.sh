#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"
MANIFEST="${ROOT_DIR}/docs/phase0/corpus_manifest.yaml"
CHECK_DIR="${ROOT_DIR}/${BUILD_DIR}/phase2"

cd "${ROOT_DIR}"
python3 tools/check_phase2_typed_hash.py
python3 tools/check_phase2_non_replay.py \
  --manifest "${MANIFEST}" \
  --output-dir "${CHECK_DIR}"

echo "Phase 2 checks passed"
