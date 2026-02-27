# Phase 10 Plan (Full Lattices + Images + Lattice Expression Language)

Date: 2026-02-24
Status: Pending (`Not Started`, queued behind Phase-9 completion)

## Objective

Implement full `lattices` + `images` capability in `casacore-mini` with
compatibility-first behavior for persisted image data and expression semantics.

This phase is persistence-complete for image/lattice workflows:

1. full lattice and image object lifecycle (create/open/read/write/update)
2. required region and mask semantics
3. required lattice expression language (LEL) parse/evaluate behavior
4. strict `casacore <-> casacore-mini` interoperability evidence

## Entry condition

Phase 10 starts only after Phase 9 is complete and passing:

1. `bash tools/check_phase9.sh` passes
2. `bash tools/check_ci_build_lint_test_coverage.sh` passes on Phase-9 code
3. `docs/phase9/exit_report.md` is complete and consistent with
   `docs/casacore_plan.md`

## Definition of done

Phase 10 is complete only when all are true:

1. the API target surface in `Phase 10 API target surface` is implemented
2. required image table persistence behavior is verified for read/write/update
3. required region and mask semantics are parity-tested
4. required LEL categories are parse/evaluate parity-tested
5. expression evaluation parity is validated on corpus-backed artifacts
6. strict matrix checks pass for all required artifacts and required cells:
   - `casacore -> casacore`
   - `casacore -> casacore-mini`
   - `casacore-mini -> casacore`
   - `casacore-mini -> casacore-mini`
7. no required checks are skipped, downgraded, or accepted as expected-fail
8. no required verifier path crashes/segfaults
9. all Phase-10 waves have complete review packets
10. all API-affecting waves include Doxygen updates

## Scope boundaries

### In scope

1. `lattices/Lattices` core model and traversal behavior needed by upstream
   image/lattice workflows:
   - lattice lifecycle, slicing/sub-lattice views, masks, iterators/cursors,
     deterministic traversal order
2. `lattices/LEL` required expression behavior:
   - parsing, AST/evaluator, scalar-lattice broadcasting, logical/comparison,
     reductions needed by current workflows
3. `images/Images` required image models:
   - `ImageInterface`, `PagedImage`, `TempImage`, `SubImage`, image metadata
4. `images/Regions` required region persistence and selection behavior
5. image/lattice interoperability fixtures and matrix coverage
6. integration with completed Phase-7/8/9 table, measure, and coordinate layers

### Out of scope

1. GUI/viewer application parity
2. performance-only optimization work not required for compatibility
3. non-required or dead-code LEL syntax paths without corpus/workflow need
4. unrelated MeasurementSet operation expansion (Phase 9)

## Phase 10 API target surface

Use this as implementation and review checklist.

| Group | Upstream module | Target classes/capabilities |
|---|---|---|
| Lattice core model | `lattices/Lattices` | `Lattice`, `MaskedLattice`, `ArrayLattice`, `PagedArray`, `SubLattice`, lattice slicing and shape APIs |
| Lattice traversal | `lattices/Lattices` | iterator/cursor behavior required by image and expression evaluation paths |
| Lattice expressions | `lattices/LEL` | `LatticeExpr`-equivalent parser/evaluator with required operators/functions |
| Image core | `images/Images` | `ImageInterface`, `PagedImage`, `TempImage`, `SubImage`, image lifecycle and metadata |
| Regions and masks | `images/Regions` | required region representations, mask persistence, region-based subsetting |
| Utility layer | `images/Images` + `lattices/LatticeMath` | required helper utilities used by interoperability and core workflows |

## Data model and dependency policy

1. multidimensional storage model is compatibility-first and modern:
   - owning storage: reference-counted aligned block
   - view model: `std::mdspan` (`layout_left` default) with fallback to
     `kokkos/mdspan` where needed
   - slicing: `submdspan` or equivalent standardized view adapters
2. no bespoke tensor container API unless required for compatibility surface
3. FITS/WCS integrations continue to use `cfitsio` and `wcslib`
4. parser/evaluator dependency decisions (if any) are locked in `P10-W1` and
   recorded in `docs/phase10/dependency_decisions.md`

## LEL coverage contract

Required expression categories for Phase-10 completion:

1. scalar/lattice arithmetic
2. comparison and logical operators
3. mask-aware boolean evaluation
4. reductions required by image/lattice workflows
5. scalar-lattice broadcasting rules used in corpus/workflow expressions
6. malformed-expression diagnostics and parse failure behavior

Each required category must include:

1. parse success/failure parity checks
2. deterministic evaluation parity vs `casacore` on required fixtures
3. malformed-expression negative tests

## Execution order and dependencies (strict)

Waves are sequential and executed in this order:

1. `P10-W1` API/corpus/LEL contract freeze + dependency lock
2. `P10-W2` lattice storage/view substrate + shape/slice foundation
3. `P10-W3` lattice core model and traversal semantics
4. `P10-W4` image core models and metadata lifecycle
5. `P10-W5` region and mask persistence semantics
6. `P10-W6` expression engine foundation (AST + evaluator core)
7. `P10-W7` required LEL parser and operator/function coverage
8. `P10-W8` image/lattice mutation flows and persistence integrity
9. `P10-W9` utility-layer parity and integration cleanup
10. `P10-W10` strict image/lattice interoperability matrix
11. `P10-W11` hardening + malformed-input corpus expansion + docs convergence
12. `P10-W12` closeout and Phase-11 handoff

No user confirmation pauses between waves during normal execution.

## Required quality and documentation gates (every wave)

Mandatory commands:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. `bash tools/check_phase10_wX.sh` for current wave

Phase aggregate command:

1. `bash tools/check_phase10.sh`

Gate rules:

1. warnings-as-errors and clang-tidy are non-optional
2. required tests may not be downgraded to informational
3. required matrix/equivalence checks may not be skipped
4. functional claims must be validated by executable tests/scripts
5. verifier crashes are automatic wave failures
6. compatibility-facing waves must include producer self-check validity
7. wave checks must use command exit status as pass/fail source of truth

## Workstreams

| ID | Status | Scope | Required deliverables |
|---|---|---|---|
| `P10-W1` | Done | Contract freeze | API map, artifact inventory, LEL coverage contract, dependency decisions, check scaffolding |
| `P10-W2` | Done | Storage/view substrate | shape, stride, slicing, alignment, shared-storage semantics |
| `P10-W3` | Done | Lattice core | lattice model classes, traversal semantics, baseline lattice persistence tests |
| `P10-W4` | Done | Image core | image interfaces/models, metadata lifecycle, table persistence integration |
| `P10-W5` | Done | Regions/masks | region representations, mask persistence, region subset behavior |
| `P10-W6` | Done | LEL evaluator foundation | expression AST and evaluator core with deterministic semantics |
| `P10-W7` | Done | LEL completeness | required parser/operator/function categories and diagnostics |
| `P10-W8` | Done | Mutation integrity | image/lattice updates, persistence roundtrips, corruption guards |
| `P10-W9` | Done | Utility parity | required utility behaviors used by image/lattice workflows |
| `P10-W10` | Done | Matrix parity | strict 2x2 interop matrix for required image/lattice artifacts |
| `P10-W11` | Done | Hardening | malformed/corrupt input hardening, docs/API convergence, coverage expansion |
| `P10-W12` | Done | Closeout | exit report, status reconciliation, Phase-11 entry gate declaration |

## Wave details (implementation-ready)

### `P10-W1` API/corpus/LEL contract freeze

Implementation tasks:

1. create API map from `casacore-original/lattices` and
   `casacore-original/images` to `casacore-mini`:
   - `docs/phase10/api_surface_map.csv`
2. define required interop artifacts and semantic checks:
   - `docs/phase10/interop_artifact_inventory.md`
   - `docs/phase10/lel_coverage_contract.md`
   - `docs/phase10/tolerances.md`
3. lock dependency decisions:
   - `docs/phase10/dependency_decisions.md`
4. scaffold checks and matrix runner:
   - `tools/check_phase10.sh`
   - `tools/check_phase10_w1.sh`
   - `tools/interop/run_phase10_matrix.sh` (skeleton)
5. lock lint profile for this phase:
   - `docs/phase10/lint_profile.md`
6. add API-surface conformance scaffolding tied to symbol and test evidence

Expected touchpoints:

1. `docs/phase10/*`
2. `tools/check_phase10*.sh`
3. `tools/interop/run_phase10_matrix.sh`

Wave gate:

1. required contracts exist and are explicit
2. API map covers all in-scope classes from this plan
3. lint profile lock exists and matches active lint config
4. API-map rows are wired to symbol/test evidence scaffolding
5. `bash tools/check_phase10_w1.sh` passes

### `P10-W2` lattice storage/view substrate

Implementation tasks:

1. implement shape/stride/view substrate with deterministic axis order
2. enforce shared-storage semantics required by lattice/view paths
3. implement slice/sub-view semantics used by lattice/image APIs
4. add alignment and overflow/size guard behavior tests

Expected touchpoints:

1. lattice storage headers/sources
2. tests for shape/slice/storage aliasing and ownership semantics

Wave gate:

1. `tools/check_phase10_w2.sh` passes
2. required storage/view semantics are behavior-tested

### `P10-W3` lattice core model and traversal

Implementation tasks:

1. implement required lattice model classes and core operations
2. implement required traversal/iterator behavior
3. add lattice persistence roundtrip tests for representative lattice types
4. add malformed-input tests for invalid shape/index/slice cases

Expected touchpoints:

1. lattice core headers/sources
2. tests under `tests/`

Wave gate:

1. `tools/check_phase10_w3.sh` passes
2. lattice traversal and persistence behavior checks pass

### `P10-W4` image core models and metadata lifecycle

Implementation tasks:

1. implement required image model classes (`ImageInterface`, `PagedImage`,
   `TempImage`, `SubImage` equivalents)
2. wire image metadata/keywords/coordinate integration
3. implement create/open/read/write/update image lifecycle behavior
4. add roundtrip tests for scalar and cube image artifacts

Expected touchpoints:

1. image core headers/sources
2. table persistence integration points
3. image lifecycle tests

Wave gate:

1. `tools/check_phase10_w4.sh` passes
2. required image lifecycle parity checks pass

### `P10-W5` region and mask persistence

Implementation tasks:

1. implement required region representations and persistence encoding
2. implement mask creation/read/update semantics
3. validate region-based subset behavior parity
4. add malformed region/mask negative tests

Expected touchpoints:

1. region/mask headers/sources
2. image/lattice integration tests

Wave gate:

1. `tools/check_phase10_w5.sh` passes
2. region/mask semantic parity checks pass

### `P10-W6` expression engine foundation

Implementation tasks:

1. implement expression AST/evaluator core
2. implement deterministic scalar-lattice broadcasting rules
3. integrate mask-aware evaluator behavior
4. add deterministic evaluation vector tests on fixed fixtures

Expected touchpoints:

1. expression core headers/sources
2. evaluator tests and fixtures

Wave gate:

1. `tools/check_phase10_w6.sh` passes
2. evaluator foundation behavior is parity-tested

### `P10-W7` required LEL parser and operator/function coverage

Implementation tasks:

1. implement required parser coverage for contracted categories
2. implement required operator/function semantics
3. add malformed-expression diagnostics parity tests
4. enforce API-map conformance for expression entries

Expected touchpoints:

1. expression parser/evaluator headers/sources
2. parser/evaluator tests and interop fixtures

Wave gate:

1. `tools/check_phase10_w7.sh` passes
2. required LEL categories have zero required skips
3. expression API-map rows are backed by symbol/test evidence

### `P10-W8` mutation flows and persistence integrity

Implementation tasks:

1. implement image/lattice update and mutation flows
2. verify post-mutation persistence roundtrip behavior
3. verify region/mask/expression interactions after mutation
4. add corruption/partial-write guard behavior tests where supported

Expected touchpoints:

1. image/lattice write/update paths
2. mutation tests and fixture generators

Wave gate:

1. `tools/check_phase10_w8.sh` passes
2. mutation roundtrip parity checks pass on required artifacts

### `P10-W9` utility-layer parity and integration cleanup

Implementation tasks:

1. implement required helper utilities needed by current workflows
2. validate utility outputs against `casacore` on fixed fixtures
3. reconcile API/docs naming and behavior consistency
4. add edge-case tests for helper behavior

Expected touchpoints:

1. utility headers/sources
2. tests and interop helpers

Wave gate:

1. `tools/check_phase10_w9.sh` passes
2. utility parity vectors pass for required cases

### `P10-W10` strict image/lattice interoperability matrix

Implementation tasks:

1. complete strict matrix runner:
   - `tools/interop/run_phase10_matrix.sh`
2. include required artifact families and semantic checks
3. ensure matrix failures are strictly gating
4. verify all 2x2 cells for each required artifact

Expected touchpoints:

1. matrix runner + interop tools
2. check scripts
3. artifact generators and verifiers

Wave gate:

1. `tools/check_phase10_w10.sh` passes
2. required matrix cells pass with no required skips/expected-fails
3. zero required verifier crashes

### `P10-W11` hardening + corpus expansion + docs convergence

Implementation tasks:

1. add malformed/corrupt-input hardening tests
2. expand corpus-backed edge-case coverage
3. converge public API docs with implemented behavior
4. verify no stale/overstated behavior claims remain

Expected touchpoints:

1. tests and fixtures
2. Doxygen comments
3. phase docs (`known_differences` if needed)

Wave gate:

1. `tools/check_phase10_w11.sh` passes
2. hardening checks pass on required malformed-input suites

### `P10-W12` closeout and Phase-11 handoff

Implementation tasks:

1. produce `docs/phase10/exit_report.md` with:
   - objective outcomes
   - matrix tables and semantic evidence
   - known differences
   - carry-forward items
2. reconcile status across:
   - `docs/phase10/plan.md`
   - `docs/phase10/exit_report.md`
   - `docs/casacore_plan.md`
3. declare Phase-11 entry conditions
4. regenerate final evidence from fresh reruns:
   - `bash tools/check_phase10.sh build-p10-closeout`
   - `bash tools/interop/run_phase10_matrix.sh build-p10-closeout`
   - `bash tools/check_ci_build_lint_test_coverage.sh build-p10-closeout-ci`

Expected touchpoints:

1. `docs/phase10/*`
2. rolling plan status updates
3. aggregate checks

Wave gate:

1. `tools/check_phase10.sh` passes end-to-end
2. no status inconsistency remains in plan/exit/rolling docs
3. closeout evidence is freshly generated and count-consistent

## Interoperability and evaluation contract (strict, no skip-pass)

### Matrix shape

Required for each required artifact:

1. `casacore -> casacore`
2. `casacore -> casacore-mini`
3. `casacore-mini -> casacore`
4. `casacore-mini -> casacore-mini`

### Required Phase-10 artifacts

At minimum:

1. minimal `PagedImage` with scalar pixel type and coordinate metadata
2. multidimensional cube image with representative mask usage
3. image with region-defined subset extraction/persistence
4. lattice-expression-generated artifact exercising required LEL categories
5. mixed scalar/complex pixel artifact where supported by upstream behavior

### Semantic pass criteria

Each matrix cell must validate:

1. schema and keyword parity
2. pixel-value parity (using `docs/phase10/tolerances.md`)
3. shape/axis/order parity
4. region/mask behavior parity
5. expression evaluation parity for required categories
6. error behavior parity for malformed inputs

Diagnostic dumps are allowed; pass/fail is semantic.

## Mandatory review packet artifacts per wave

For each `P10-WX`, commit:

1. `docs/phase10/review/P10-WX/summary.md`
2. `docs/phase10/review/P10-WX/files_changed.txt`
3. `docs/phase10/review/P10-WX/check_results.txt`
4. `docs/phase10/review/P10-WX/matrix_results.json` (or analogous structured
   file for non-matrix waves)
5. `docs/phase10/review/P10-WX/open_issues.md`
6. `docs/phase10/review/P10-WX/decisions.md`

Closure rule:

1. no wave is `Done` unless all required review artifacts exist
2. no Phase-10 completion claim unless required matrix artifacts pass all 2x2
   cells
3. no required capability may be marked pass through skip/tolerated-failure
4. no completion claim if any required verifier path crashes

## Carry-forward guardrails

Phase 10 must explicitly avoid known miss patterns:

1. do not claim expression parity from parser-only checks; evaluate values
2. do not claim compatibility from metadata-only image checks
3. do not treat skipped required checks as pass
4. keep `plan`/`exit_report`/rolling-plan status text synchronized
5. require malformed-input tests for new parsers/decoders
6. treat verifier crash bugs as blocking defects
7. keep wave checks exit-status-driven
8. lock lint profile at phase start; document any changes with rationale
9. require API-map row-to-symbol and row-to-test traceability

## Immediate next step

After Phase-9 completion is merged, execute `P10-W1` and commit:

1. `docs/phase10/api_surface_map.csv`
2. `docs/phase10/interop_artifact_inventory.md`
3. `docs/phase10/lel_coverage_contract.md`
4. `docs/phase10/tolerances.md`
5. `docs/phase10/dependency_decisions.md`
6. `docs/phase10/lint_profile.md`
7. `tools/check_phase10.sh`
8. `tools/check_phase10_w1.sh`
9. `tools/interop/run_phase10_matrix.sh`

## Autonomy policy (implementation)

The implementing agent should execute all waves sequentially without waiting
for user input during normal flow. Stop only for:

1. external dependency installation failure blocking compilation
2. hard incompatibility requiring explicit scope change
3. data/licensing constraints preventing required fixture creation

When blocked, document in current wave review packet and continue independent
work where possible.
