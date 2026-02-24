# P8-W1 Summary

## Scope implemented

- API surface map (`api_surface_map.csv`) covering all 48 in-scope upstream classes
  mapped to casacore-mini files, types, and target waves.
- Dependency decisions (`dependency_decisions.md`) locking ERFA 2.0.1 (BSD) and
  WCSLIB 8.x (LGPL-3.0) with pkg-config detection and vendored fallback for ERFA.
- Interop artifact inventory (`interop_artifact_inventory.md`) defining 6 required
  artifacts for the 24-cell matrix.
- Reference data policy (`reference_data_policy.md`) defining vendored snapshots,
  discovery order, failure semantics, and `MissingReferenceDataError`.
- Numeric tolerance specifications (`tolerances.md`) for all 9 measure types and
  6 coordinate types.
- Phase check scaffolding: `check_phase8.sh`, `check_phase8_w1.sh`,
  `interop/run_phase8_matrix.sh` (skeleton with 24 SKIP cells).
- CMakeLists.txt: ERFA and WCSLIB detection via pkg-config, vendored ERFA fallback,
  `CASACORE_MINI_DATA_DIR_DEFAULT` compile definition.
- README.md: build prerequisites section with ERFA and WCSLIB installation instructions.
- CI gate: `check_phase8.sh` added to `check_ci_build_lint_test_coverage.sh`.

## Public API changes

None (W1 is documentation and tooling only).

## Behavioral changes

- CMake now requires ERFA and WCSLIB at configure time. Both detected via pkg-config.
  ERFA falls back to vendored source in `third_party/erfa/` if available.
- `FATAL_ERROR` if either dependency is missing (clear install instructions in message).

## Deferred items

None.
