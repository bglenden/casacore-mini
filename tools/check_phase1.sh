#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-build-quality}"
MANIFEST="${ROOT_DIR}/docs/phase0/corpus_manifest.yaml"
CHECK_DIR="${ROOT_DIR}/${BUILD_DIR}/phase1"

cd "${ROOT_DIR}"

if [[ ! -x "${ROOT_DIR}/${BUILD_DIR}/table_schema_cli" ]]; then
  echo "table_schema_cli not found in ${BUILD_DIR}; build target before running phase1 checks"
  exit 1
fi

mkdir -p "${CHECK_DIR}/oracle"

python3 tools/oracle_dump.py \
  --manifest "${MANIFEST}" \
  --artifact-id replay_ttableaccess \
  --output-dir "${CHECK_DIR}/oracle" \
  --strict-checksum

python3 tools/check_phase1_schema_hook.py \
  --table-schema-cli "${ROOT_DIR}/${BUILD_DIR}/table_schema_cli" \
  --showtableinfo-file "${ROOT_DIR}/data/corpus/fixtures/replay_ttableaccess/showtableinfo.txt" \
  --oracle-json "${CHECK_DIR}/oracle/replay_ttableaccess.oracle.json"

echo "Phase 1 schema hook check passed"
