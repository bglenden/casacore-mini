#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2026 Brian Glendenning
# SPDX-License-Identifier: LGPL-3.0-or-later
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "${ROOT_DIR}"

TAG_NAME="${1:-${GITHUB_REF_NAME:-}}"

if [[ -z "${TAG_NAME}" ]]; then
  echo "No tag provided; release tag check skipped"
  exit 0
fi

if [[ ! "${TAG_NAME}" =~ ^v([0-9]+\.[0-9]+\.[0-9]+)$ ]]; then
  echo "Release tags must match vX.Y.Z; got '${TAG_NAME}'"
  exit 1
fi

TAG_VERSION="${BASH_REMATCH[1]}"

PROJECT_LINE="$(grep -E '^project\(casacore_mini VERSION [0-9]+\.[0-9]+\.[0-9]+' CMakeLists.txt | head -n 1 || true)"
if [[ -z "${PROJECT_LINE}" ]]; then
  echo "Unable to read project version from CMakeLists.txt"
  exit 1
fi

PROJECT_VERSION="$(echo "${PROJECT_LINE}" | sed -E 's/^project\(casacore_mini VERSION ([0-9]+\.[0-9]+\.[0-9]+).*/\1/')"

if [[ "${TAG_VERSION}" != "${PROJECT_VERSION}" ]]; then
  echo "Tag version (${TAG_VERSION}) does not match project version (${PROJECT_VERSION})"
  exit 1
fi

echo "Release tag '${TAG_NAME}' matches project version '${PROJECT_VERSION}'"
