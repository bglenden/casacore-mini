#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

REPO_ROOT="${DEFAULT_REPO_ROOT}"
NATIVE_BUILD_DIR="build-preflight-native"
LINUX_BUILD_DIR="build-preflight-linux"
SKIP_LINUX=0
HOST_FORMAT=0

print_help() {
  cat <<'EOF'
Usage: bash tools/preflight_worktree_merge.sh [options]

Run merge-gate checks for a worktree branch before fast-forwarding into main.

Options:
  --repo-root <path>        Repository/worktree root to validate
  --native-build-dir <name> Build dir for host fast checks
  --linux-build-dir <name>  Build dir for Linux container checks
  --skip-linux              Skip Linux container parity lane
  --host-format             Also run host clang-format check
  --help                    Show this help text
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo-root)
      REPO_ROOT="$(cd "$2" && pwd)"
      shift 2
      ;;
    --native-build-dir)
      NATIVE_BUILD_DIR="$2"
      shift 2
      ;;
    --linux-build-dir)
      LINUX_BUILD_DIR="$2"
      shift 2
      ;;
    --skip-linux)
      SKIP_LINUX=1
      shift
      ;;
    --host-format)
      HOST_FORMAT=1
      shift
      ;;
    --help|-h)
      print_help
      exit 0
      ;;
    *)
      echo "Unknown argument: $1" >&2
      print_help
      exit 1
      ;;
  esac
done

if [[ ! -d "${REPO_ROOT}" ]]; then
  echo "Repository root does not exist: ${REPO_ROOT}" >&2
  exit 1
fi

if [[ ! -f "${REPO_ROOT}/tools/check_ci_fast_build_test.sh" ]]; then
  echo "Expected script missing in repo root: ${REPO_ROOT}/tools/check_ci_fast_build_test.sh" >&2
  exit 1
fi

echo "Preflight repo root: ${REPO_ROOT}"

if [[ "${HOST_FORMAT}" == "1" ]]; then
  bash "${REPO_ROOT}/tools/check_format.sh"
fi
bash "${REPO_ROOT}/tools/check_ci_fast_build_test.sh" "${NATIVE_BUILD_DIR}"

if [[ "${SKIP_LINUX}" == "1" ]]; then
  echo "Skipping Linux container parity lane (--skip-linux)"
  if [[ "${HOST_FORMAT}" != "1" ]]; then
    echo "Note: format parity was also skipped (run with --host-format or Linux lane enabled)."
  fi
else
  bash "${SCRIPT_DIR}/check_ci_fast_build_test_linux_container.sh" \
    --repo-root "${REPO_ROOT}" \
    --build-dir "${LINUX_BUILD_DIR}"
fi

echo "Worktree merge preflight passed"
