#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

cd "${ROOT_DIR}"

echo "Phase 10 W1 gate: API/corpus/LEL contract freeze"

# 1. Verify all W1 documentation files exist and are non-empty.
REQUIRED_DOCS=(
  "docs/phase10/api_surface_map.csv"
  "docs/phase10/interop_artifact_inventory.md"
  "docs/phase10/lel_coverage_contract.md"
  "docs/phase10/tolerances.md"
  "docs/phase10/dependency_decisions.md"
  "docs/phase10/lint_profile.md"
)

for doc in "${REQUIRED_DOCS[@]}"; do
  if [ ! -s "${doc}" ]; then
    echo "FAIL: ${doc} missing or empty" >&2
    exit 1
  fi
done

# 2. Verify API surface map covers all required upstream classes.
REQUIRED_CLASSES=(
  "Lattice<T>"
  "MaskedLattice<T>"
  "ArrayLattice<T>"
  "PagedArray<T>"
  "SubLattice<T>"
  "TempLattice<T>"
  "LatticeIterator<T>"
  "LatticeExpr<T>"
  "LatticeExprNode"
  "LELInterface<T>"
  "LELBinary"
  "LELUnary"
  "LELFunction"
  "ImageInterface<T>"
  "PagedImage<T>"
  "TempImage<T>"
  "SubImage<T>"
  "ImageExpr<T>"
  "ImageInfo"
  "LCRegion"
  "LCBox"
  "LCPolygon"
  "LCEllipsoid"
  "LCMask"
  "LCIntersection"
  "LCUnion"
  "WCRegion"
  "WCBox"
  "WCPolygon"
  "ImageRegion"
  "RegionManager"
)

for cls in "${REQUIRED_CLASSES[@]}"; do
  if ! grep -qF "${cls}" docs/phase10/api_surface_map.csv; then
    echo "FAIL: API surface map missing class: ${cls}" >&2
    exit 1
  fi
done

# 3. Verify interop artifact inventory specifies 5 required artifacts.
ARTIFACT_COUNT=$(grep -c "^## Artifact" docs/phase10/interop_artifact_inventory.md || true)
if [ "${ARTIFACT_COUNT}" -lt 5 ]; then
  echo "FAIL: interop_artifact_inventory.md has ${ARTIFACT_COUNT} artifacts, expected >= 5" >&2
  exit 1
fi

# 4. Verify LEL coverage contract covers all 8 categories.
for cat_num in 1 2 3 4 5 6 7 8; do
  if ! grep -q "## Category ${cat_num}" docs/phase10/lel_coverage_contract.md; then
    echo "FAIL: lel_coverage_contract.md missing Category ${cat_num}" >&2
    exit 1
  fi
done

# 5. Verify tolerances document covers key value types.
for ttype in "Float pixels" "Double pixels" "Complex pixels" "Expression evaluation" "Shape and mask"; do
  if ! grep -q "${ttype}" docs/phase10/tolerances.md; then
    echo "FAIL: tolerances.md missing tolerance for: ${ttype}" >&2
    exit 1
  fi
done

# 6. Verify dependency decisions document exists and covers key decisions.
for dep in "mdspan" "parser" "evaluator" "persistence"; do
  if ! grep -qi "${dep}" docs/phase10/dependency_decisions.md; then
    echo "FAIL: dependency_decisions.md missing decision for: ${dep}" >&2
    exit 1
  fi
done

# 7. Verify lint profile lock exists and references clang-tidy.
if ! grep -q "clang-tidy" docs/phase10/lint_profile.md; then
  echo "FAIL: lint_profile.md missing clang-tidy reference" >&2
  exit 1
fi

# 8. Verify phase check scripts exist.
if [ ! -f tools/check_phase10.sh ]; then
  echo "FAIL: tools/check_phase10.sh not found" >&2
  exit 1
fi

if [ ! -f tools/interop/run_phase10_matrix.sh ]; then
  echo "FAIL: tools/interop/run_phase10_matrix.sh not found" >&2
  exit 1
fi

echo "Phase 10 W1 gate passed"
