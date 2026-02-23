# casacore-mini

`casacore-mini` is a modernization-focused reimplementation of key `casacore` capabilities in modern C++, with minimal bespoke infrastructure.

## Purpose

This repository aims to:

- Preserve interoperability with persistent data structures created by existing `casacore`.
- Keep and modernize radio-astronomy-specific functionality such as:
  - measures
  - coordinates
  - images
  - table-based persistence and related classes
- Replace generic legacy infrastructure (for example custom array/container/runtime utilities) with standard C++ or widely used libraries when they provide equivalent capability.

## Current Status

Initial architecture and rolling plan documents are in:

- `docs/casacore_plan.md`
- `docs/module_inventory.csv`

## Engineering Quality Gates

The repository enforces quality from the start:

- Formatting: `clang-format` (CI checked)
- Compiler warnings: strict warning set with `-Werror`
- Lint: `clang-tidy` (CI enforced)
- Tests: `ctest` required in CI
- Coverage: line coverage gate on `src/` (currently `>= 70%`)

CI workflow: `.github/workflows/quality.yml`.

## Scope Direction (Initial)

Priority is compatibility-critical read/write semantics for table-based persistence, then progressive implementation of measures, coordinates, images, and MeasurementSet-related functionality.

## Non-Goals (Initial)

- Full one-to-one API parity with all historical `casacore` modules in the first release.
- Retaining bespoke infrastructure where modern standard/library alternatives are better maintained and widely adopted.
