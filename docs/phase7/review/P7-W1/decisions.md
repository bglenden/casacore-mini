# P7-W1 Decisions

## Autonomous decisions made

- Used `pkg-config --cflags casacore` / `pkg-config --libs casacore` for
  `casacore_interop_tool` compilation rather than a CMake `find_package` path,
  keeping the casacore-side tool outside the project CMake build graph.

## Assumptions adopted

- The matrix runner assumes casacore is installed and discoverable via
  `pkg-config`. This is validated at the top of `run_phase7_matrix.sh`.

## Tradeoffs accepted

- `casacore_interop_tool` is compiled as a standalone TU rather than integrated
  into the CMake build. This avoids coupling the project build to casacore
  headers/libs while still exercising true casacore code paths.
