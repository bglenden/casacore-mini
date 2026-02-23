#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build}"
LINE_THRESHOLD="${2:-70}"

cd "${ROOT_DIR}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "gcovr is required but was not found"
  exit 1
fi

if ! find src -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.c' \) | grep -q .; then
  echo "No source files under src/; coverage check skipped"
  exit 0
fi

gcovr \
  --root "${ROOT_DIR}" \
  --object-directory "${BUILD_DIR}" \
  --filter "${ROOT_DIR}/src" \
  --exclude "${ROOT_DIR}/tests" \
  --txt-summary \
  --fail-under-line "${LINE_THRESHOLD}"
