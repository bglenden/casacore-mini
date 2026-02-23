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

Initial architecture audit and staged implementation plan are in:

- `docs/casacore_audit.md`
- `docs/module_inventory.csv`

## Scope Direction (Initial)

Priority is compatibility-critical read/write semantics for table-based persistence, then progressive implementation of measures, coordinates, images, and MeasurementSet-related functionality.

## Non-Goals (Initial)

- Full one-to-one API parity with all historical `casacore` modules in the first release.
- Retaining bespoke infrastructure where modern standard/library alternatives are better maintained and widely adopted.
