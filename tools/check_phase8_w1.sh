#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${ROOT_DIR}"

echo "Phase 8 W1 gate: API/dependency/fixture contract"

# 1. Verify all W1 documentation files exist and are non-empty.
REQUIRED_DOCS=(
  "docs/phase8/api_surface_map.csv"
  "docs/phase8/dependency_decisions.md"
  "docs/phase8/interop_artifact_inventory.md"
  "docs/phase8/reference_data_policy.md"
  "docs/phase8/tolerances.md"
)

for doc in "${REQUIRED_DOCS[@]}"; do
  if [ ! -s "${doc}" ]; then
    echo "FAIL: ${doc} missing or empty" >&2
    exit 1
  fi
done

# 2. Verify API surface map covers all required upstream classes.
REQUIRED_CLASSES=(
  "MEpoch"
  "MDirection"
  "MPosition"
  "MFrequency"
  "MDoppler"
  "MRadialVelocity"
  "MBaseline"
  "Muvw"
  "MEarthMagnetic"
  "MeasureHolder"
  "VelocityMachine"
  "UVWMachine"
  "ParAngleMachine"
  "EarthMagneticMachine"
  "TableMeasDesc"
  "ScalarMeasColumn"
  "ArrayMeasColumn"
  "TableQuantumDesc"
  "ScalarQuantColumn"
  "ArrayQuantColumn"
  "Coordinate"
  "CoordinateSystem"
  "DirectionCoordinate"
  "SpectralCoordinate"
  "StokesCoordinate"
  "LinearCoordinate"
  "TabularCoordinate"
  "QualityCoordinate"
  "Projection"
  "LinearXform"
  "ObsInfo"
  "CoordinateUtil"
  "FITSCoordinateUtil"
  "GaussianConvert"
)

for cls in "${REQUIRED_CLASSES[@]}"; do
  if ! grep -q "${cls}" docs/phase8/api_surface_map.csv; then
    echo "FAIL: API surface map missing class: ${cls}" >&2
    exit 1
  fi
done

# 3. Verify dependency decisions mention ERFA and WCSLIB.
if ! grep -q "ERFA" docs/phase8/dependency_decisions.md; then
  echo "FAIL: dependency_decisions.md missing ERFA decision" >&2
  exit 1
fi
if ! grep -q "WCSLIB" docs/phase8/dependency_decisions.md; then
  echo "FAIL: dependency_decisions.md missing WCSLIB decision" >&2
  exit 1
fi

# 4. Verify reference data policy mentions vendored snapshots and failure semantics.
if ! grep -q "MissingReferenceDataError" docs/phase8/reference_data_policy.md; then
  echo "FAIL: reference_data_policy.md missing MissingReferenceDataError" >&2
  exit 1
fi
if ! grep -q "CASACORE_MINI_DATA_DIR" docs/phase8/reference_data_policy.md; then
  echo "FAIL: reference_data_policy.md missing CASACORE_MINI_DATA_DIR" >&2
  exit 1
fi

# 5. Verify interop artifact inventory specifies 6 required artifacts.
ARTIFACT_COUNT=$(grep -c "^## Artifact" docs/phase8/interop_artifact_inventory.md || true)
if [ "${ARTIFACT_COUNT}" -lt 6 ]; then
  echo "FAIL: interop_artifact_inventory.md has ${ARTIFACT_COUNT} artifacts, expected >= 6" >&2
  exit 1
fi

# 6. Verify tolerances document covers all measure types.
for mtype in "Epoch" "Direction" "Position" "Frequency" "Doppler"; do
  if ! grep -q "${mtype}" docs/phase8/tolerances.md; then
    echo "FAIL: tolerances.md missing tolerance for ${mtype}" >&2
    exit 1
  fi
done

# 7. Verify phase check scripts exist.
if [ ! -f tools/check_phase8.sh ]; then
  echo "FAIL: tools/check_phase8.sh not found" >&2
  exit 1
fi

if [ ! -f tools/interop/run_phase8_matrix.sh ]; then
  echo "FAIL: tools/interop/run_phase8_matrix.sh not found" >&2
  exit 1
fi

# 8. Verify CMakeLists.txt has ERFA and WCSLIB dependency blocks.
if ! grep -q "ERFA\|erfa" CMakeLists.txt; then
  echo "FAIL: CMakeLists.txt missing ERFA dependency block" >&2
  exit 1
fi
if ! grep -q "WCSLIB\|wcslib" CMakeLists.txt; then
  echo "FAIL: CMakeLists.txt missing WCSLIB dependency block" >&2
  exit 1
fi

echo "Phase 8 W1 gate passed"
