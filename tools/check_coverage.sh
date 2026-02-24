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

if [[ -n "${GCOV_EXECUTABLE:-}" ]]; then
  gcov_executable="${GCOV_EXECUTABLE}"
elif command -v llvm-cov >/dev/null 2>&1; then
  gcov_executable="llvm-cov gcov"
elif command -v gcov >/dev/null 2>&1; then
  gcov_executable="gcov"
else
  echo "Neither llvm-cov nor gcov is available for coverage processing"
  exit 1
fi

if ! find src -type f \( -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' -o -name '*.c' \) | grep -q .; then
  echo "No source files under src/; coverage check skipped"
  exit 0
fi

# Restrict discovery to the selected build directory to avoid merging
# stale coverage artifacts from other local build trees.
gcovr "${BUILD_DIR}" \
  --root "${ROOT_DIR}" \
  --object-directory "${BUILD_DIR}" \
  --gcov-executable "${gcov_executable}" \
  --filter "${ROOT_DIR}/src" \
  --exclude "${ROOT_DIR}/src/.*_tool\\.cpp$" \
  --exclude "${ROOT_DIR}/src/.*_cli\\.cpp$" \
  --exclude "${ROOT_DIR}/src/aipsio_fixture_gen\\.cpp$" \
  --exclude "${ROOT_DIR}/tests" \
  --txt-summary \
  --fail-under-line "${LINE_THRESHOLD}"
