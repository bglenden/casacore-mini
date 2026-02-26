#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${ROOT_DIR}"

echo "Phase 9 W1 gate: API/corpus/contract freeze"

# 1. Verify all W1 documentation files exist and are non-empty.
REQUIRED_DOCS=(
  "docs/phase9/api_surface_map.csv"
  "docs/phase9/interop_artifact_inventory.md"
  "docs/phase9/selection_coverage_contract.md"
  "docs/phase9/operations_coverage_contract.md"
  "docs/phase9/tolerances.md"
  "docs/phase9/lint_profile.md"
)

for doc in "${REQUIRED_DOCS[@]}"; do
  if [ ! -s "${doc}" ]; then
    echo "FAIL: ${doc} missing or empty" >&2
    exit 1
  fi
done

# 2. Verify API surface map covers all required upstream classes.
REQUIRED_CLASSES=(
  "MeasurementSet"
  "MSTable"
  "MSAntenna"
  "MSDataDescription"
  "MSDoppler"
  "MSFeed"
  "MSField"
  "MSFlagCmd"
  "MSFreqOffset"
  "MSHistory"
  "MSObservation"
  "MSPointing"
  "MSPolarization"
  "MSProcessor"
  "MSSource"
  "MSSpectralWindow"
  "MSState"
  "MSSysCal"
  "MSWeather"
  "MSMainColumns"
  "MSColumns"
  "MSIter"
  "MSRange"
  "StokesConverter"
  "MSDopplerUtil"
  "MSTileLayout"
  "MSHistoryHandler"
  "MSSelection"
  "MSSelectionKeywords"
  "MSSelector"
  "MSMetaData"
  "MSReader"
  "MSSummary"
  "MSDerivedValues"
  "MSConcat"
  "MSFlagger"
  "MSValidIds"
  "MSKeys"
  "MSLister"
)

for cls in "${REQUIRED_CLASSES[@]}"; do
  if ! grep -q "${cls}" docs/phase9/api_surface_map.csv; then
    echo "FAIL: API surface map missing class: ${cls}" >&2
    exit 1
  fi
done

# 3. Verify interop artifact inventory specifies 5 required artifacts.
ARTIFACT_COUNT=$(grep -c "^## Artifact" docs/phase9/interop_artifact_inventory.md || true)
if [ "${ARTIFACT_COUNT}" -lt 5 ]; then
  echo "FAIL: interop_artifact_inventory.md has ${ARTIFACT_COUNT} artifacts, expected >= 5" >&2
  exit 1
fi

# 4. Verify selection coverage contract covers all 8 categories.
for cat_num in 1 2 3 4 5 6 7 8; do
  if ! grep -q "## Category ${cat_num}" docs/phase9/selection_coverage_contract.md; then
    echo "FAIL: selection_coverage_contract.md missing Category ${cat_num}" >&2
    exit 1
  fi
done

# 5. Verify operations coverage contract covers required operations.
for op in "MsMetaData" "MsSummary" "MsConcat" "MsFlagger" "MsWriter" "MsIter" "StokesConverter"; do
  if ! grep -q "${op}" docs/phase9/operations_coverage_contract.md; then
    echo "FAIL: operations_coverage_contract.md missing operation: ${op}" >&2
    exit 1
  fi
done

# 6. Verify tolerances document covers key value types.
for ttype in "Double-precision" "Single-precision" "Integer" "String" "Timestamp"; do
  if ! grep -q "${ttype}" docs/phase9/tolerances.md; then
    echo "FAIL: tolerances.md missing tolerance for: ${ttype}" >&2
    exit 1
  fi
done

# 7. Verify lint profile lock exists and references clang-tidy.
if ! grep -q "clang-tidy" docs/phase9/lint_profile.md; then
  echo "FAIL: lint_profile.md missing clang-tidy reference" >&2
  exit 1
fi

# 8. Verify phase check scripts exist.
if [ ! -f tools/check_phase9.sh ]; then
  echo "FAIL: tools/check_phase9.sh not found" >&2
  exit 1
fi

if [ ! -f tools/interop/run_phase9_matrix.sh ]; then
  echo "FAIL: tools/interop/run_phase9_matrix.sh not found" >&2
  exit 1
fi

# 9. Verify review packet template exists.
if [ ! -s docs/phase9/review_packet_template.md ]; then
  echo "FAIL: docs/phase9/review_packet_template.md missing or empty" >&2
  exit 1
fi

echo "Phase 9 W1 gate passed"
