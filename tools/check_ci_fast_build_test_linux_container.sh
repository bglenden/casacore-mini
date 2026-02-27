#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DEFAULT_REPO_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

REPO_ROOT="${DEFAULT_REPO_ROOT}"
BUILD_DIR="build-ci-fast-linux-local"
CONTAINER_ENGINE="${CONTAINER_ENGINE:-docker}"
IMAGE_TAG="${CASACORE_MINI_CI_FAST_IMAGE:-casacore-mini/ci-fast:ubuntu24.04}"
DOCKERFILE_PATH="${CASACORE_MINI_CI_FAST_DOCKERFILE:-${SCRIPT_DIR}/docker/ci-fast.Dockerfile}"
DOCKER_CONTEXT_DIR="${CASACORE_MINI_CI_FAST_DOCKER_CONTEXT:-${SCRIPT_DIR}/docker}"
REBUILD_IMAGE=0

print_help() {
  cat <<'EOF'
Usage: bash tools/check_ci_fast_build_test_linux_container.sh [options]

Run fast CI parity checks inside a Linux container (clang + libstdc++).

Options:
  --repo-root <path>   Repository/worktree root to test (default: current repo)
  --build-dir <name>   Build directory passed to check_ci_fast_build_test.sh
  --rebuild-image      Force image rebuild before running checks
  --help               Show this help text

Environment:
  CONTAINER_ENGINE                 Container CLI (default: docker)
  CASACORE_MINI_CI_FAST_IMAGE      Image tag
  CASACORE_MINI_CI_FAST_DOCKERFILE Dockerfile path
  CASACORE_MINI_CI_FAST_DOCKER_CONTEXT Build context path
  CASACORE_MINI_FORMAT_SCOPE       Passed into container for format scope
EOF
}

while [[ $# -gt 0 ]]; do
  case "$1" in
    --repo-root)
      REPO_ROOT="$(cd "$2" && pwd)"
      shift 2
      ;;
    --build-dir)
      BUILD_DIR="$2"
      shift 2
      ;;
    --rebuild-image)
      REBUILD_IMAGE=1
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

if ! command -v "${CONTAINER_ENGINE}" >/dev/null 2>&1; then
  echo "${CONTAINER_ENGINE} was not found on PATH." >&2
  echo "Install Docker/Podman or set CONTAINER_ENGINE to an available CLI." >&2
  exit 1
fi

if [[ ! -d "${REPO_ROOT}" ]]; then
  echo "Repository root does not exist: ${REPO_ROOT}" >&2
  exit 1
fi

if [[ ! -f "${REPO_ROOT}/tools/check_ci_fast_build_test.sh" ]]; then
  echo "Expected script missing in repo root: ${REPO_ROOT}/tools/check_ci_fast_build_test.sh" >&2
  exit 1
fi

if [[ ! -f "${DOCKERFILE_PATH}" ]]; then
  echo "Dockerfile not found: ${DOCKERFILE_PATH}" >&2
  exit 1
fi

need_build=0
if [[ "${REBUILD_IMAGE}" == "1" ]]; then
  need_build=1
elif ! "${CONTAINER_ENGINE}" image inspect "${IMAGE_TAG}" >/dev/null 2>&1; then
  need_build=1
fi

if [[ "${need_build}" == "1" ]]; then
  echo "Building Linux parity image: ${IMAGE_TAG}"
  "${CONTAINER_ENGINE}" build \
    -f "${DOCKERFILE_PATH}" \
    -t "${IMAGE_TAG}" \
    "${DOCKER_CONTEXT_DIR}"
fi

uid="$(id -u)"
gid="$(id -g)"
format_scope="${CASACORE_MINI_FORMAT_SCOPE:-changed}"

echo "Running Linux fast-parity checks in container (${CONTAINER_ENGINE})"
"${CONTAINER_ENGINE}" run --rm \
  --user "${uid}:${gid}" \
  -e HOME=/tmp \
  -e CASACORE_MINI_FORMAT_SCOPE="${format_scope}" \
  -e CASACORE_MINI_REPO_ROOT="${REPO_ROOT}" \
  -v "${REPO_ROOT}:${REPO_ROOT}" \
  -w "${REPO_ROOT}" \
  "${IMAGE_TAG}" \
  bash -lc "git config --global --add safe.directory \"\$CASACORE_MINI_REPO_ROOT\" >/dev/null 2>&1 || true; bash tools/check_format.sh && bash tools/check_ci_fast_build_test.sh ${BUILD_DIR}"

echo "Linux container parity checks passed"
