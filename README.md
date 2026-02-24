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
- `docs/phase1/` (`plan`, `exit_report`)
- `docs/phase2/` (`plan`, `exit_report`)
- `docs/phase3/` (`plan`, `measure_coord_contract_v0`, `exit_report`)
- `docs/phase4/` (`plan`, `exit_report`)
- `docs/phase5/` (`plan`, `keyword_record_fidelity_v1`, `exit_report`)
- `docs/phase6/` (`plan`)
- `docs/phase7/` (`plan`)
- `docs/module_inventory.csv`
- `docs/phase0/` (`baselines`, compatibility contract, corpus manifest, and exit report)

## Engineering Quality Gates

The repository enforces quality from the start:

- Formatting: `clang-format` (CI checked)
- Compiler warnings: strict warning set with `-Werror`
- Lint: `clang-tidy` (CI enforced)
- Tests: `ctest` required in CI
- API documentation: Doxygen HTML generation required in CI
- Coverage: line coverage gate on `src/` (currently `>= 70%`)

CI workflows:

- Fast (`push`/`pull_request`): `.github/workflows/quality.yml`
- Full (`nightly`/`manual`/`release tag`): `.github/workflows/quality-full.yml`

Local developer workflow: `CONTRIBUTING.md`.

Key local check scripts:

- `tools/check_ci_fast_build_test.sh` (matches CI fast `build-test` job)
- `tools/check_ci_build_lint_test_coverage.sh` (matches CI `build-lint-test-coverage` job)
- `tools/check_docs.sh` (matches CI `docs` job)
- `tools/run_quality.sh` (format + build/lint/test/coverage + docs)
- `tools/pre_push_quality.sh` (intended for pre-push use)
- `tools/install_git_hooks.sh` (installs a `pre-push` hook that runs `tools/pre_push_quality.sh`)

## Endianness Policy

- Host architecture support is currently limited to little-endian systems.
- A compile-time guard enforces this in `include/casacore_mini/platform.hpp`.
- This does not change on-disk compatibility goals: `AipsIO` canonical bytes are
  still treated as big-endian in read paths.

## API Documentation

The repository includes optional Doxygen HTML generation from public headers.

- Template: `docs/Doxyfile.in`
- Build target: `doc`
- Output directory: `build*/docs/doxygen/html/`

Example:

```bash
cmake -S . -B build -G Ninja
cmake --build build --target doc
```

The `quality-full` CI workflow validates Doxygen HTML generation in nightly,
manual, and release-tag runs.

## Pre-push guardrail (recommended)

To reduce CI churn and failure emails, install the repository pre-push hook:

```bash
bash tools/install_git_hooks.sh
```

This runs `tools/pre_push_quality.sh` before every push, which executes the
full-quality checks (`format`, `build-lint-test-coverage`, and `docs`) locally
before upload.

## Phase 0 Interop Tooling

Phase 0 interoperability artifacts and tooling are in:

- Corpus manifest: `docs/phase0/corpus_manifest.yaml`
- Replay fixtures: `data/corpus/`
- Oracle extractor: `tools/oracle_dump.py`
- Comparator: `tools/oracle_compare.py`
- Manifest validator: `tools/validate_corpus_manifest.py`
- Combined phase check: `tools/check_phase0.sh`

The CI quality workflow runs `tools/check_phase0.sh` before compile/test steps.

## Phase 7 Interop Matrix Harness (Scoped)

Scoped bidirectional `casacore` <-> `casacore-mini` interop checks are run via:

```bash
bash tools/interop/run_phase7_matrix.sh
```

This executes a 2x2 producer/consumer matrix for currently implemented
artifacts (`AipsIO Record` + `table.dat` header scope). Pass/fail is based on
semantic read/verify checks in both consumers. Canonical ASCII dumps are kept
as diagnostics and are emitted automatically on verification failure.

Prerequisite (macOS/Homebrew):

```bash
brew tap casacore/tap
brew install casacore
```

## Versioning

Versioning is semantic (`MAJOR.MINOR.PATCH`) with Git tags as release truth.

- Canonical source version is `project(casacore_mini VERSION X.Y.Z)` in `CMakeLists.txt`.
- Releases are tagged as `vX.Y.Z` on GitHub.
- CI enforces tag/version consistency (`tools/check_release_tag.sh`).
- Build metadata is derived from git at CMake configure time:
  - `project_version()` -> canonical `X.Y.Z`
  - `build_version()` -> release `X.Y.Z` on matching tag, otherwise `X.Y.Z+<git-describe>` (or `X.Y.Z-dev` if git metadata is unavailable)
  - `git_describe()` / `git_revision()` expose source provenance
  - `version()` is an alias for `build_version()`

This means source builds can be traced to an exact commit while keeping a stable release version model.

## Scope Direction (Initial)

Priority is compatibility-critical read/write semantics for table-based persistence, then progressive implementation of measures, coordinates, images, and MeasurementSet-related functionality.

## Non-Goals (Initial)

- Full one-to-one API parity with all historical `casacore` modules in the first release.
- Retaining bespoke infrastructure where modern standard/library alternatives are better maintained and widely adopted.
