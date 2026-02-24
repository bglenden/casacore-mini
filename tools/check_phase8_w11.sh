#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-phase8}"

echo "=== Phase 8 Wave 11 Gate: Hardening + Corpus Expansion ==="

PASS=0
FAIL=0

check() {
  local label="$1"
  shift
  if "$@" >/dev/null 2>&1; then
    echo "  PASS: ${label}"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: ${label}" >&2
    FAIL=$((FAIL + 1))
  fi
}

# --- Test files exist ---
for f in tests/measure_malformed_test.cpp \
         tests/coordinate_malformed_test.cpp \
         tests/reference_data_absence_test.cpp; do
  check "file exists: ${f}" test -f "${ROOT_DIR}/${f}"
done

# --- Known differences doc ---
check "file exists: docs/phase8/known_differences.md" \
  test -f "${ROOT_DIR}/docs/phase8/known_differences.md"

# --- Tests pass ---
for t in measure_malformed_test coordinate_malformed_test reference_data_absence_test; do
  if ctest --test-dir "${ROOT_DIR}/${BUILD_DIR}" -R "^${t}$" --output-on-failure 2>&1 \
     | grep -q "100% tests passed"; then
    echo "  PASS: ${t}"
    PASS=$((PASS + 1))
  else
    echo "  FAIL: ${t}" >&2
    FAIL=$((FAIL + 1))
  fi
done

echo ""
echo "=== W11 gate: PASS=${PASS} FAIL=${FAIL} ==="
if [ "${FAIL}" -gt 0 ]; then
  echo "W11 gate FAILED" >&2
  exit 1
fi
echo "W11 gate passed."
