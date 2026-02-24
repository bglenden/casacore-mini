# P7-W1 Summary

## Scope implemented

Matrix scaffold for Phase 7 semantic interoperability testing:

- `tools/interop/run_phase7_matrix.sh`: end-to-end matrix runner that builds
  both tools, produces artifacts from each producer, and verifies with both
  consumers (2x2 matrix).
- `tools/interop/casacore_interop_tool.cpp`: standalone casacore-linked tool
  for producing, verifying, and dumping AipsIO artifacts.
- `src/interop_mini_tool.cpp`: casacore-mini-linked counterpart with matching
  subcommand set.
- CMake wiring for `interop_mini_tool` build target.

## Public API changes

None. All scaffold components are tools/test infrastructure, not library API.

## Behavioral changes

None. No existing behavior modified.

## Deferred items (if any)

None. Scaffold only; no artifact cases exercised in this wave.
