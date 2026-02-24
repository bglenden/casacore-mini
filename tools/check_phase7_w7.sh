#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-ci-local}"

cd "${ROOT_DIR}"

echo "Phase 7 W7 gate: tiled manager interop"

# Verify the tiled directory test passes.
"${BUILD_DIR}/tiled_directory_test"

# Verify deferred managers documentation exists.
if [ ! -f "docs/phase7/deferred_managers.md" ]; then
  echo "FAIL: docs/phase7/deferred_managers.md not found" >&2
  exit 1
fi

echo "Phase 7 W7 gate passed"
