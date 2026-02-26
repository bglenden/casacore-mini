# Phase 11 Plan (Remaining Capabilities Closure)

Date: 2026-02-24
Status: Pending (`Not Started`, queued behind Phase-10 completion)

## Objective

Complete all accepted remaining `casacore` capabilities not closed by
Phases 1-10, with an audit-first, compatibility-first execution model.

Phase 11 is planned as the terminal delivery phase:

1. complete an explicit include/exclude audit of remaining capability surface
2. implement all accepted capabilities with executable parity evidence
3. run full cross-domain interoperability closure tests
4. perform a full storage-manager fidelity audit to identify and remove
   simplifications/heuristics versus upstream casacore behavior
5. publish final compatibility status, known differences, and maintenance backlog

## Entry condition

Phase 11 starts only after Phase 10 is complete and passing:

1. `bash tools/check_phase10.sh` passes
2. `bash tools/check_ci_build_lint_test_coverage.sh` passes on Phase-10 code
3. `docs/phase10/exit_report.md` is complete and consistent with
   `docs/casacore_plan.md`

## Definition of done

Phase 11 is complete only when all are true:

1. `P11-W1` produces a complete remaining-capabilities audit with explicit
   include/exclude decisions and rationale
2. every capability marked `Include` in `P11-W1` is implemented and
   compatibility-tested by phase closeout
3. every capability marked `Exclude` has explicit rationale and impact statement
4. all required Phase-11 wave gates pass with no required skips
5. full project interoperability matrix passes for required cross-domain
   artifacts (`Record`, tables, measures, coordinates, MeasurementSet,
   lattice/image)
6. no required verifier path crashes/segfaults
7. no required checks are downgraded to informational
8. all Phase-11 waves have complete review packets
9. all API-affecting waves include Doxygen updates
10. `docs/casacore_plan.md` and all phase plans/exit reports are status-consistent

## Scope boundaries

### In scope

1. audit and closure of remaining capability surface from modules not completed
   in Phases 1-10, including candidates from:
   - `scimath`
   - `scimath_f`
   - `meas` (including required UDF/query-adjacent surfaces)
   - `fits`/`msfits` conversion surfaces
   - `derivedmscal`
   - residual utility domains required for compatibility claims
2. implementation of all capabilities accepted in `P11-W1`
3. full-system interoperability closure suite and final evidence
4. final known-differences and non-goals publication

### Out of scope

1. GUI/application parity outside core library behavior
2. performance-only optimization work not required for correctness parity
3. adding new product scope beyond upstream compatibility targets

## Phase 11 capability closure contract

`P11-W1` produces the authoritative closure contract:

1. `docs/phase11/missing_capabilities_audit.csv`
2. `docs/phase11/disposition_decisions.md`
3. `docs/phase11/api_surface_map.csv`
4. `docs/phase11/interop_artifact_inventory.md`
5. `docs/phase11/tolerances.md`
6. `docs/phase11/storage_manager_fidelity_audit.md`

Audit rows must include at minimum:

1. upstream module/path
2. upstream capability/class/function identifier
3. persistence/interoperability impact (`Critical`, `High`, `Medium`, `Low`)
4. workflow impact (`Required`, `NiceToHave`, `OutOfScope`)
5. disposition (`Include`, `Exclude`)
6. target wave assignment (`P11-W3`..`P11-W7` for included capabilities)
7. rationale and dependency notes
8. simplification status (`Exact`, `Simplified`) and remediation wave
   assignment for non-`Exact` rows

## Execution order and dependencies (strict)

Waves are sequential and executed in this order:

1. `P11-W1` remaining-capabilities audit + include/exclude freeze
2. `P11-W2` closure contract lock + tranche planning + check scaffolding
3. `P11-W3` implementation tranche A (highest-priority included capabilities)
4. `P11-W4` implementation tranche B
5. `P11-W5` implementation tranche C
6. `P11-W6` implementation tranche D
7. `P11-W7` implementation tranche E + gap closure
8. `P11-W8` cross-tranche API consistency and integration convergence
9. `P11-W9` capability-specific interoperability matrix
10. `P11-W10` full-project interoperability closure matrix
11. `P11-W11` hardening + stress + documentation convergence
12. `P11-W12` final closeout and long-term maintenance handoff

No user confirmation pauses between waves during normal execution.

## Required quality and documentation gates (every wave)

Mandatory commands:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. `bash tools/check_phase11_wX.sh` for current wave

Phase aggregate command:

1. `bash tools/check_phase11.sh`

Gate rules:

1. warnings-as-errors and clang-tidy are non-optional
2. required tests may not be downgraded to informational
3. required matrix/equivalence checks may not be skipped
4. functional claims must be validated by executable tests/scripts
5. verifier crashes are automatic wave failures
6. wave checks must use command exit status as pass/fail source of truth
7. compatibility-facing gates require producer self-check validity

## Workstreams

| ID | Status | Scope | Required deliverables |
|---|---|---|---|
| `P11-W1` | Pending | Audit + disposition freeze | complete capability inventory, include/exclude decisions, wave assignments, storage-manager simplification audit |
| `P11-W2` | Pending | Contract lock | frozen tranche plan, check scaffolding, lint profile lock, API-conformance scaffolding |
| `P11-W3` | Pending | Tranche A | highest-priority included capabilities implemented + tested |
| `P11-W4` | Pending | Tranche B | next tranche implemented + tested |
| `P11-W5` | Pending | Tranche C | next tranche implemented + tested |
| `P11-W6` | Pending | Tranche D | next tranche implemented + tested |
| `P11-W7` | Pending | Tranche E + closure | final included capabilities implemented + tested |
| `P11-W8` | Pending | Integration convergence | API consistency, cross-tranche wiring, behavior/doc alignment |
| `P11-W9` | Pending | Phase-specific matrix | strict 2x2 matrix for Phase-11-added capabilities |
| `P11-W10` | Pending | Full-system matrix | strict full-project 2x2 matrix with detailed artifacts |
| `P11-W11` | Pending | Hardening | malformed/corrupt input hardening, stress tests, docs convergence |
| `P11-W12` | Pending | Final closeout | final exit report, known-differences ledger, release/maintenance handoff |

## Wave details (implementation-ready)

### `P11-W1` remaining-capabilities audit + disposition freeze

Implementation tasks:

1. inventory all remaining upstream capability surfaces not closed by P1-P10
2. classify each capability by interoperability/workflow impact
3. assign `Include` or `Exclude` with explicit rationale
4. assign each included capability to a target implementation tranche wave
5. audit all storage-manager/table-format reader+writer paths for behavioral
   simplifications/heuristics and classify each finding as `Exact` or `Simplified`
6. produce and commit audit artifacts:
   - `docs/phase11/missing_capabilities_audit.csv`
   - `docs/phase11/disposition_decisions.md`
   - `docs/phase11/api_surface_map.csv`
   - `docs/phase11/storage_manager_fidelity_audit.md`

Expected touchpoints:

1. `docs/phase11/*`
2. references to prior phase exit reports and known-differences docs

Wave gate:

1. all in-scope upstream capability families are represented in audit rows
2. each row has disposition, rationale, and wave assignment (if included)
3. storage-manager fidelity audit exists and every `Simplified` item has an
   explicit remediation wave or explicit exclusion rationale
4. `tools/check_phase11_w1.sh` passes

### `P11-W2` closure contract lock + tranche planning

Implementation tasks:

1. lock tranche execution mapping for included capabilities (`W3`..`W7`)
2. create/check phase scaffolding:
   - `tools/check_phase11.sh`
   - `tools/check_phase11_w2.sh`
   - skeleton `tools/interop/run_phase11_matrix.sh`
3. create phase contracts:
   - `docs/phase11/interop_artifact_inventory.md`
   - `docs/phase11/tolerances.md`
   - `docs/phase11/lint_profile.md`
4. create API-conformance scaffolding tying API-map rows to symbol/test evidence

Expected touchpoints:

1. `docs/phase11/*`
2. `tools/check_phase11*.sh`
3. `tools/interop/run_phase11_matrix.sh`

Wave gate:

1. tranche mapping is explicit and complete
2. lint profile lock is present and consistent with active lint config
3. API-conformance scaffolding exists
4. `tools/check_phase11_w2.sh` passes

### `P11-W3` implementation tranche A

Implementation tasks:

1. implement tranche-A included capabilities from `P11-W2` mapping
2. add parity tests and malformed-input tests for tranche-A surface
3. update Doxygen for new public APIs
4. ensure API-map row traceability for tranche-A entries

Expected touchpoints:

1. module headers/sources for tranche-A capabilities
2. tests and fixtures

Wave gate:

1. `tools/check_phase11_w3.sh` passes
2. tranche-A capabilities have behavior-based tests and parity evidence

### `P11-W4` implementation tranche B

Implementation tasks:

1. implement tranche-B included capabilities
2. add parity and negative-path tests
3. update Doxygen/API docs
4. enforce API-map traceability for tranche-B entries

Expected touchpoints:

1. tranche-B module headers/sources
2. tests and fixtures

Wave gate:

1. `tools/check_phase11_w4.sh` passes
2. tranche-B parity checks pass

### `P11-W5` implementation tranche C

Implementation tasks:

1. implement tranche-C included capabilities
2. add parity and malformed-input tests
3. update docs and API map evidence links

Expected touchpoints:

1. tranche-C module headers/sources
2. tests and fixtures

Wave gate:

1. `tools/check_phase11_w5.sh` passes
2. tranche-C parity checks pass

### `P11-W6` implementation tranche D

Implementation tasks:

1. implement tranche-D included capabilities
2. add parity and malformed-input tests
3. update docs and API map evidence links

Expected touchpoints:

1. tranche-D module headers/sources
2. tests and fixtures

Wave gate:

1. `tools/check_phase11_w6.sh` passes
2. tranche-D parity checks pass

### `P11-W7` implementation tranche E + gap closure

Implementation tasks:

1. implement tranche-E included capabilities
2. close any remaining included capability gaps from audit rows
3. verify every `Include` row has symbol and exercising-test evidence
4. update docs and known differences draft

Expected touchpoints:

1. tranche-E module headers/sources
2. tests and fixtures
3. `docs/phase11/api_surface_map.csv`

Wave gate:

1. `tools/check_phase11_w7.sh` passes
2. no included capability row remains unimplemented

### `P11-W8` integration convergence

Implementation tasks:

1. reconcile cross-tranche API consistency and naming conventions
2. ensure integration with P7-P10 stacks in active paths
3. remove stale/dead compatibility claims
4. add integration tests spanning multiple capability families

Expected touchpoints:

1. cross-module integration headers/sources
2. integration tests and docs updates

Wave gate:

1. `tools/check_phase11_w8.sh` passes
2. integration tests demonstrate active-path coverage

### `P11-W9` capability-specific interoperability matrix

Implementation tasks:

1. complete strict phase matrix runner for added Phase-11 capabilities
2. execute all required 2x2 cells for Phase-11 artifact families
3. enforce matrix failures as strict gate failures
4. produce structured matrix results for review packet

Expected touchpoints:

1. `tools/interop/run_phase11_matrix.sh`
2. interop artifact generators/verifiers
3. wave check scripts

Wave gate:

1. `tools/check_phase11_w9.sh` passes
2. required Phase-11 matrix cells pass with no required skips
3. zero required verifier crashes

### `P11-W10` full-project interoperability closure matrix

Implementation tasks:

1. implement full-project matrix runner:
   - tables, measures, coordinates, MeasurementSet, lattice/image,
     and Phase-11-added capabilities
2. generate detailed producer artifacts from both toolchains
3. verify cross-reader semantic equivalence on detailed artifacts
4. gate phase progression on full-project matrix pass

Expected touchpoints:

1. project-level interop scripts and tools
2. full-system fixtures and verifiers
3. wave check scripts

Wave gate:

1. `tools/check_phase11_w10.sh` passes
2. full-project required matrix cells all pass
3. detailed artifact semantic checks pass (not byte-layout-only checks)

### `P11-W11` hardening + stress + docs convergence

Implementation tasks:

1. add malformed/corrupt-input hardening tests for all newly added parsers/I/O
2. add stress and scale tests for critical paths
3. converge Doxygen, plan docs, and behavior claims
4. publish/update `docs/phase11/known_differences.md`

Expected touchpoints:

1. tests and fixtures
2. Doxygen/public headers
3. phase docs

Wave gate:

1. `tools/check_phase11_w11.sh` passes
2. hardening suites pass and docs are behavior-consistent

### `P11-W12` final closeout and maintenance handoff

Implementation tasks:

1. produce `docs/phase11/exit_report.md` with:
   - objective outcomes
   - disposition outcomes (`Include` implemented, `Exclude` rationale)
   - full-project matrix tables and evidence
   - known differences and explicit non-goals
   - post-Phase-11 maintenance backlog
2. reconcile status across:
   - `docs/phase11/plan.md`
   - `docs/phase11/exit_report.md`
   - `docs/casacore_plan.md`
3. regenerate final evidence from fresh reruns:
   - `bash tools/check_phase11.sh build-p11-closeout`
   - `bash tools/interop/run_phase11_matrix.sh build-p11-closeout`
   - `bash tools/check_ci_build_lint_test_coverage.sh build-p11-closeout-ci`
4. publish final compatibility statement and entry point for post-phase
   maintenance (no new phase predeclared)

Expected touchpoints:

1. `docs/phase11/*`
2. rolling plan status updates
3. aggregate checks and matrix evidence

Wave gate:

1. `tools/check_phase11.sh` passes end-to-end
2. full-project matrix evidence is fresh and count-consistent
3. plan/exit/rolling documents are status-consistent
4. no unresolved `Include` capability remains open

## Interoperability and evaluation contract (strict, no skip-pass)

### Matrix shape

Required for each required artifact family:

1. `casacore -> casacore`
2. `casacore -> casacore-mini`
3. `casacore-mini -> casacore`
4. `casacore-mini -> casacore-mini`

### Required full-project artifact families (minimum)

1. rich `Record` artifacts with nested and array content
2. full table artifacts across required storage managers
3. measure/coordinate-rich metadata artifacts
4. MeasurementSet artifacts with required subtables and operations coverage
5. image/lattice artifacts with masks/regions/expressions
6. Phase-11-added capability-specific artifacts from audit contract

### Semantic pass criteria

Each required matrix cell must validate:

1. schema and keyword parity
2. value and shape parity with documented tolerances
3. linkage/association integrity where applicable
4. operation-level behavior parity for required scenarios
5. malformed-input error behavior parity for required failure cases

Diagnostic dumps are allowed; pass/fail is semantic.

## Mandatory review packet artifacts per wave

For each `P11-WX`, commit:

1. `docs/phase11/review/P11-WX/summary.md`
2. `docs/phase11/review/P11-WX/files_changed.txt`
3. `docs/phase11/review/P11-WX/check_results.txt`
4. `docs/phase11/review/P11-WX/matrix_results.json` (or analogous structured
   file for non-matrix waves)
5. `docs/phase11/review/P11-WX/open_issues.md`
6. `docs/phase11/review/P11-WX/decisions.md`

Closure rule:

1. no wave is `Done` unless all required review artifacts exist
2. no Phase-11 completion claim unless required full-project matrix artifacts
   pass all required cells
3. no required capability may be marked pass through skip/tolerated-failure
4. no completion claim if any required verifier path crashes

## Carry-forward guardrails

Phase 11 must explicitly avoid known miss patterns:

1. do not claim completion while any `Include` audit rows remain open
2. do not treat skipped required checks as pass
3. do not claim parity without corpus-backed matrix evidence
4. keep plan/exit/rolling status synchronized
5. require malformed-input tests for all new parsers/writers
6. treat verifier crash bugs as blocking defects
7. keep wave checks exit-status-driven
8. lock lint profile at phase start; document any mid-phase changes
9. require API-map row-to-symbol and row-to-test traceability
10. require full-project matrix rerun on current commit before closeout

## Immediate next step

After Phase-10 completion is merged, execute `P11-W1` and commit:

1. `docs/phase11/missing_capabilities_audit.csv`
2. `docs/phase11/disposition_decisions.md`
3. `docs/phase11/api_surface_map.csv`
4. `tools/check_phase11_w1.sh`

Then execute `P11-W2` and commit:

1. `docs/phase11/interop_artifact_inventory.md`
2. `docs/phase11/tolerances.md`
3. `docs/phase11/lint_profile.md`
4. `tools/check_phase11.sh`
5. `tools/check_phase11_w2.sh`
6. `tools/interop/run_phase11_matrix.sh`

## Autonomy policy (implementation)

The implementing agent should execute all waves sequentially without waiting
for user input during normal flow. Stop only for:

1. external dependency installation failure blocking compilation
2. hard incompatibility requiring explicit scope change
3. data/licensing constraints preventing required fixture creation

When blocked, document in current wave review packet and continue independent
work where possible.
