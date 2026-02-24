# Phase 8 Plan (Full Measures + Coordinates + CoordinateSystems)

Date: 2026-02-24
Status: Pending (`Not Started`, queued behind final Phase-7 closeout)

## Objective

Implement full `Measures`, `TableMeasures`, `Coordinates`, and
`CoordinateSystem` functionality in `casacore-mini` with strict interoperability
against `casacore`.

This phase is compatibility-first and persistence-aware:

1. public APIs are implemented with modernized naming/style where appropriate
2. persisted table/image metadata and measure/coordinate records are fully
   readable/writable by both stacks
3. numeric conversion behavior is verified with deterministic tolerances

## Entry condition

Phase 8 execution starts only after Phase 7 passes strict closeout gates:

1. `bash tools/check_ci_build_lint_test_coverage.sh` passes
2. `bash tools/check_phase7.sh` passes with no required matrix skips
3. Phase-7 status/docs are internally consistent

## Definition of done

Phase 8 is complete only when all are true:

1. the target class surface listed in `Phase 8 API target surface` is
   implemented and wired into active code paths
2. bidirectional `casacore <-> casacore-mini` matrix checks pass for required
   measures/coordinate artifacts
3. semantic parity checks pass for:
   - measure type/reference metadata
   - measure values and frame semantics
   - coordinate-system keyword representation
   - world/pixel conversion behavior with documented tolerances
   - external reference-data behavior (`IERS`/leap-second/ephemeris-backed
     conversions) with documented provenance
4. no required checks are skipped or marked informational-only
5. no verifier crash/segfault occurs in required phase checks or required
   matrix cells
6. all Phase-8 waves have complete review packets
7. public API documentation is updated (Doxygen) for every API-affecting wave

## Scope boundaries

### In scope

1. `measures/Measures` core measure types and conversion/frame machinery:
   - `Measure`, `MeasRef`, `MeasFrame`, `MeasureHolder`
   - `MEpoch`, `MPosition`, `MDirection`, `MFrequency`, `MDoppler`,
     `MRadialVelocity`, `MBaseline`, `Muvw`, `MEarthMagnetic`
   - conversion helpers used by public APIs:
     `VelocityMachine`, `UVWMachine`, `EarthMagneticMachine`,
     `ParAngleMachine`
2. `measures/TableMeasures` descriptors and column adaptors:
   - `TableMeasType`, `TableMeasDescBase`, `TableMeasDesc`,
     `TableMeasRefDesc`, `TableMeasValueDesc`, `TableMeasOffsetDesc`
   - `TableMeasColumn`, `ScalarMeasColumn`, `ArrayMeasColumn`
   - `TableQuantumDesc`, `ScalarQuantColumn`, `ArrayQuantColumn`
3. `coordinates/Coordinates`:
   - `Coordinate`, `CoordinateSystem`
   - `DirectionCoordinate`, `SpectralCoordinate`, `StokesCoordinate`,
     `LinearCoordinate`, `TabularCoordinate`, `QualityCoordinate`
   - `Projection`, `LinearXform`, `ObsInfo`, `CoordinateUtil`,
     `FITSCoordinateUtil`, `GaussianConvert`
4. interoperability hooks in both interop tools and matrix harnesses
5. corpus fixture expansion required for full compatibility claims
6. reference-data integration behavior for measures conversions, including:
   - Earth orientation (`IERS`) data use
   - leap-second handling
   - ephemeris-backed paths required by in-scope conversions
   - deterministic behavior when reference data is unavailable or stale

### Out of scope

1. TaQL UDF layer under `meas/MeasUDF` (tracked under later phase)
2. MeasurementSet full-feature operations (Phase 9)
3. lattice/image expression language (Phase 10)
4. optional legacy utilities not required by the above public surfaces, unless
   discovered as hard dependency during implementation

## Phase 8 API target surface (explicit)

Use this as the completion checklist and review reference. Coverage is keyed to
upstream modules under local `casacore-original/`.

| Group | Upstream module | Target classes/capabilities |
|---|---|---|
| Measures core | `measures/Measures` | `Measure`, `MeasRef`, `MeasFrame`, `MeasureHolder`, all required `M*` types listed above, frame-aware conversions |
| Table-bound measures | `measures/TableMeasures` | descriptor classes, scalar/array measure columns, scalar/array quantum columns, keyword encoding/decoding |
| Coordinate primitives | `coordinates/Coordinates` | `Coordinate`, `Projection`, `LinearXform`, `ObsInfo` |
| Coordinate types | `coordinates/Coordinates` | `DirectionCoordinate`, `SpectralCoordinate`, `StokesCoordinate`, `LinearCoordinate`, `TabularCoordinate`, `QualityCoordinate` |
| Coordinate systems | `coordinates/Coordinates` | `CoordinateSystem`, axis mapping, combined world/pixel transforms, record serialization |
| Interop utilities | `coordinates/Coordinates` | `CoordinateUtil`, `FITSCoordinateUtil`, `GaussianConvert` required public behavior |

## External dependency policy (minimal bespoke infrastructure)

Phase 8 must prefer established libraries instead of bespoke math engines:

1. use `ERFA` for astronomical time/frame transforms where applicable
2. use `WCSLIB` for FITS/WCS projection behavior
3. use existing project `Record` + table persistence paths for serialized
   metadata representation
4. no phase completion if conversion/projection core remains ad-hoc bespoke code
   where the above libraries already provide behavior

Dependency finalization occurs in `P8-W1` and is locked in
`docs/phase8/dependency_decisions.md`.

## Reference data policy (IERS/leap seconds/ephemerides)

Phase 8 must explicitly define and test third-party reference-data behavior.

Required policy decisions (in `P8-W1`):

1. data sources and accepted upstream formats for:
   - `IERS` Earth-orientation parameters
   - leap-second tables
   - ephemeris data used by in-scope measure conversions
2. deterministic test strategy:
   - vendored snapshot fixtures for CI/review reproducibility
   - explicit source provenance and version/date capture
3. runtime behavior:
   - data discovery order
   - cache/load behavior
   - failure semantics when data missing/invalid/stale
4. compatibility target:
   - parity with casacore behavior for the same reference-data snapshot
   - explicitly documented known differences where exact parity is impossible

## Execution order and dependencies (strict)

Waves are sequential and must be executed in this order:

1. `P8-W1` API surface freeze + dependency lock + fixture contract
2. `P8-W2` measure core value/reference/frame data model + serialization
3. `P8-W3` primary measure types (`MEpoch`, `MPosition`, `MDirection`)
4. `P8-W4` remaining measure types + conversion machines
5. `P8-W5` `TableMeasures` descriptors and column integration
6. `P8-W6` coordinate primitives and projection foundation
7. `P8-W7` concrete coordinate classes implementation
8. `P8-W8` `CoordinateSystem` composition and conversions
9. `P8-W9` utility layer (`CoordinateUtil`, `FITSCoordinateUtil`,
   `GaussianConvert`) and persistence integration
10. `P8-W10` full Phase-8 interop matrix implementation
11. `P8-W11` malformed-input hardening + corpus expansion + docs convergence
12. `P8-W12` closeout evidence + carry-forward updates

No user confirmation pauses between waves during normal execution.

## Required quality and documentation gates (every wave)

Mandatory commands:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. `bash tools/check_phase8_wX.sh` for current wave

Phase aggregate command:

1. `bash tools/check_phase8.sh`

Gate rules:

1. warnings-as-errors and clang-tidy are non-optional
2. required tests may not be downgraded to informational
3. required matrix/equivalence checks may not be skipped
4. wave gates must validate behavior by executing tools/tests; grep/string-only
   proxies are not acceptable for functional claims
5. verifier crashes/segfaults are automatic wave failures
6. compatibility-facing waves must include producer self-check validity
   (`casacore->casacore`, `casacore-mini->casacore-mini`) in addition to
   cross-check validity

## Workstreams

| ID | Status | Scope | Required deliverables |
|---|---|---|---|
| `P8-W1` | Pending | API/dependency/fixture contract | API target map, dependency decisions, fixture inventory, phase check scaffolding |
| `P8-W2` | Pending | Measure core model + serialization | core measure/ref/frame types, Record/AipsIO representation, parser/writer guards |
| `P8-W3` | Pending | Primary measure types | `MEpoch`, `MPosition`, `MDirection` read/write + conversion parity tests |
| `P8-W4` | Pending | Remaining measure types + machines | `MFrequency`, `MDoppler`, `MRadialVelocity`, `MBaseline`, `Muvw`, `MEarthMagnetic`, machine helpers |
| `P8-W5` | Pending | TableMeasures integration | descriptor classes + scalar/array measure/quantum column adapters integrated with table stack |
| `P8-W6` | Pending | Coordinate primitives | `Coordinate`, `Projection`, `LinearXform`, `ObsInfo` persistence and transform primitives |
| `P8-W7` | Pending | Concrete coordinate classes | Direction/Spectral/Stokes/Linear/Tabular/Quality coordinate implementations |
| `P8-W8` | Pending | CoordinateSystem | coordinate composition, axis mapping, world/pixel path parity, record serialization |
| `P8-W9` | Pending | Coordinate utility layer | `CoordinateUtil`, `FITSCoordinateUtil`, `GaussianConvert` behavior needed by interoperability |
| `P8-W10` | Pending | Interop matrix | strict 2x2 producer/consumer matrix for all required Phase-8 artifacts |
| `P8-W11` | Pending | Hardening + coverage | malformed/corrupt-input tests, corpus-backed stress cases, docs/API alignment checks |
| `P8-W12` | Pending | Closeout + handoff | exit report, misses/carry-forward updates, Phase-9 entry gate declaration |

## Wave details (implementation-ready)

### `P8-W1` API/dependency/fixture contract

Implementation tasks:

1. create explicit API map from `casacore-original` classes to
   `casacore-mini` modules:
   - `docs/phase8/api_surface_map.csv`
2. lock dependency decisions and rationale:
   - `docs/phase8/dependency_decisions.md`
3. define deterministic fixture corpus for measures/coordinates:
   - `docs/phase8/interop_artifact_inventory.md`
4. define reference-data policy and fixture provenance:
   - `docs/phase8/reference_data_policy.md`
5. scaffold phase checks and matrix runner placeholders:
   - `tools/check_phase8.sh`
   - `tools/check_phase8_w1.sh`
   - `tools/interop/run_phase8_matrix.sh` (initial skeleton)
6. define gate design conventions for Phase 8 checks:
   - behavior-based assertions only
   - fail-fast on crashes
   - explicit matrix-cell accounting

Expected touchpoints:

1. `docs/phase8/*`
2. `tools/check_phase8*.sh`
3. `tools/interop/run_phase8_matrix.sh`
4. `README.md` (if new prerequisites are introduced)

Wave gate:

1. API map includes all in-scope classes from this plan
2. dependency choices are explicit and justified
3. reference-data policy is explicit and testable
4. `bash tools/check_phase8_w1.sh` passes
5. W1 check scripts contain no grep/string-only proxy checks for behavioral
   claims

### `P8-W2` measure core model + serialization

Implementation tasks:

1. implement base measure model and references (frame/type/reference/value)
2. implement deterministic serialization to/from `Record` and `AipsIO` records
3. enforce malformed input guards:
   - invalid enum/reference tokens
   - size overflow
   - invalid nested record structure
4. add tests for round-trip fidelity and negative cases

Expected touchpoints:

1. new measure core headers/sources under `include/casacore_mini/` and `src/`
2. existing record conversion modules as needed
3. tests under `tests/`

Wave gate:

1. `tools/check_phase8_w2.sh` verifies serialization round trips and invalid
   input behavior
2. Doxygen comments present for public APIs introduced in W2

### `P8-W3` primary measure types (`MEpoch`, `MPosition`, `MDirection`)

Implementation tasks:

1. implement these measure types on top of W2 core model
2. implement frame-aware conversion operations against `ERFA` where required
3. add deterministic numeric parity tests versus `casacore` outputs for fixed
   test vectors
4. include conversion cases that require reference-data snapshots (for example
   frame conversions sensitive to Earth orientation/leap-second state)
5. wire interop tool commands for writing/verifying these measure artifacts

Expected touchpoints:

1. measure-type headers/sources
2. interop tools (`src/interop_mini_tool.cpp`,
   `tools/interop/casacore_interop_tool.cpp`)
3. tests and fixture generators

Wave gate:

1. `tools/check_phase8_w3.sh` passes
2. numeric parity checks satisfy tolerance spec (see `docs/phase8/tolerances.md`)
3. reference-data-backed conversion vectors pass against pinned fixtures

### `P8-W4` remaining measure types + conversion machines

Implementation tasks:

1. implement `MFrequency`, `MDoppler`, `MRadialVelocity`, `MBaseline`, `Muvw`,
   `MEarthMagnetic`
2. implement required machine helpers:
   - `VelocityMachine`
   - `UVWMachine`
   - `EarthMagneticMachine`
   - `ParAngleMachine`
3. add fixed-vector conversion parity tests against `casacore`
4. include machine-helper cases that exercise external reference-data paths
   where applicable
5. add malformed-input and edge-case tests (invalid refs, unsupported frames)

Expected touchpoints:

1. measure machine/type headers and sources
2. tests under `tests/`
3. phase checks and interop tool commands

Wave gate:

1. `tools/check_phase8_w4.sh` passes with parity checks
2. no required conversion case left unimplemented in API map
3. required reference-data-backed machine cases pass

### `P8-W5` `TableMeasures` descriptor + column integration

Implementation tasks:

1. implement descriptor classes:
   - `TableMeasType`
   - `TableMeasDescBase`, `TableMeasDesc`, `TableMeasRefDesc`,
     `TableMeasValueDesc`, `TableMeasOffsetDesc`
2. implement column adapters:
   - `TableMeasColumn`
   - `ScalarMeasColumn`, `ArrayMeasColumn`
   - `TableQuantumDesc`, `ScalarQuantColumn`, `ArrayQuantColumn`
3. bind descriptor metadata to table keyword read/write paths already present in
   Phase 7 table implementation
4. verify table and column keyword parity for measure metadata

Expected touchpoints:

1. new table-measures headers/sources
2. `table_desc`, `table_directory`, interop tools
3. fixtures and tests for measure columns

Wave gate:

1. `tools/check_phase8_w5.sh` passes
2. bidirectional keyword-level parity checks pass for required artifacts

### `P8-W6` coordinate primitives and projection foundation

Implementation tasks:

1. implement:
   - `Coordinate`
   - `Projection`
   - `LinearXform`
   - `ObsInfo`
2. implement record serialization/deserialization for these primitives
3. wire `WCSLIB` projection behavior where applicable
4. add deterministic tests for primitive world/pixel operations

Expected touchpoints:

1. coordinate primitive headers/sources
2. metadata conversion modules
3. tests and fixtures

Wave gate:

1. `tools/check_phase8_w6.sh` passes
2. projection/obsinfo record round-trip tests pass with no required skips

### `P8-W7` concrete coordinate classes

Implementation tasks:

1. implement:
   - `DirectionCoordinate`
   - `SpectralCoordinate`
   - `StokesCoordinate`
   - `LinearCoordinate`
   - `TabularCoordinate`
   - `QualityCoordinate`
2. implement pixel-to-world and world-to-pixel paths with deterministic
   tolerance checks
3. add record and keyword conversion support
4. add malformed-record tests for each concrete coordinate class

Expected touchpoints:

1. coordinate class headers/sources
2. tests and interop fixtures
3. interop tool command expansion

Wave gate:

1. `tools/check_phase8_w7.sh` passes
2. class-by-class conversion parity vectors pass

### `P8-W8` `CoordinateSystem` composition

Implementation tasks:

1. implement `CoordinateSystem` composition over concrete coordinates
2. implement axis mapping/reordering/subselection behaviors used in upstream
   workflows
3. implement full record serialization for mixed coordinate systems
4. verify world/pixel conversion parity for mixed systems against `casacore`

Expected touchpoints:

1. coordinate-system headers/sources
2. tests and fixtures for composed systems
3. interop tools and matrix contract updates

Wave gate:

1. `tools/check_phase8_w8.sh` passes
2. composed-system parity suite passes with documented tolerance budget

### `P8-W9` utility layer and persistence integration

Implementation tasks:

1. implement required behavior from:
   - `CoordinateUtil`
   - `FITSCoordinateUtil`
   - `GaussianConvert`
2. integrate with table/image metadata persistence paths
3. add compatibility tests using coordinate-rich fixtures (including paged image
   metadata forms)
4. document any intentionally unsupported utility calls with rationale

Expected touchpoints:

1. utility headers/sources
2. interop tools
3. docs (`docs/phase8/known_differences.md` if needed)

Wave gate:

1. `tools/check_phase8_w9.sh` passes
2. required utility behavior for interoperability is demonstrated by tests

### `P8-W10` strict Phase-8 interoperability matrix

Implementation tasks:

1. implement strict matrix runner:
   - `tools/interop/run_phase8_matrix.sh`
2. add matrix cases covering:
   - scalar measure columns
   - array measure columns
   - table/column measure keywords
   - coordinate-system keyword records (direction/spectral/stokes/linear/tabular)
   - world/pixel conversion probes for deterministic coordinates
   - reference-data-backed measure conversion probes using pinned snapshots
3. emit machine-readable matrix output with explicit semantic categories
4. fail on any matrix-cell failure; no expected-fail bypasses
5. ensure verifiers accept valid casacore-produced artifacts (no assumptions
   restricted to mini-produced layouts)

Expected touchpoints:

1. matrix runner + interop tools
2. check scripts
3. review artifact generation scripts (if added)

Wave gate:

1. `tools/check_phase8_w10.sh` passes
2. matrix shows all required cells and zero required skips
3. required producer self-check cells pass for every required artifact
4. zero verifier crashes across all required matrix cells

### `P8-W11` hardening + corpus expansion + docs convergence

Implementation tasks:

1. expand fixtures/corpus for edge cases:
   - reference frame variants
   - non-trivial coordinate projections
   - variable-shape coordinate metadata records
   - reference-data absence/staleness/corruption scenarios
2. add malformed-input stress tests for measure/coordinate parsers
3. add explicit tests for missing/invalid reference-data behavior
4. align all public API docs with actual behavior/throws/fidelity semantics
5. enforce review-packet completeness checker for Phase 8 waves
6. add crash-focused robustness tests for interoperability verifiers

Expected touchpoints:

1. data fixtures
2. tests + parser/writer guard code
3. docs and check scripts

Wave gate:

1. `tools/check_phase8_w11.sh` passes
2. all in-progress wave review packets pass completeness checks
3. missing/stale/corrupt reference-data behavior is verified and documented

### `P8-W12` closeout and Phase-9 handoff

Implementation tasks:

1. produce `docs/phase8/exit_report.md` with:
   - objective results
   - matrix pass/fail tables
   - known differences
   - misses observed + carry-forward actions
2. reconcile status across:
   - `docs/phase8/plan.md`
   - `docs/phase8/exit_report.md`
   - `docs/casacore_plan.md`
3. declare Phase 9 entry conditions explicitly
4. regenerate final evidence from fresh clean reruns (no reused prior outputs):
   - `bash tools/check_phase8.sh`
   - `bash tools/interop/run_phase8_matrix.sh`
   - `bash tools/check_ci_build_lint_test_coverage.sh`

Expected touchpoints:

1. Phase-8 docs
2. rolling plan updates for misses/carry-forward
3. aggregate phase checks

Wave gate:

1. `tools/check_phase8.sh` passes end-to-end
2. no status inconsistency remains in plan/exit/rolling docs
3. review packet command outputs for W10-W12 are freshly regenerated and match
   observed pass/fail counts

## Interoperability and evaluation contract (strict, no skip-pass)

### Matrix shape

Required matrix for each required artifact:

1. `casacore -> casacore`
2. `casacore -> casacore-mini`
3. `casacore-mini -> casacore`
4. `casacore-mini -> casacore-mini`

### Required Phase-8 artifacts

At minimum:

1. scalar measure-column table (multiple measure types)
2. array measure-column table
3. table with rich table/column measure keywords
4. coordinate-rich image/table metadata artifact
5. mixed coordinate-system artifact with direction + spectral + stokes +
   linear/tabular components
6. reference-data-sensitive conversion artifact set (pinned snapshots + expected
   outcomes)

### Semantic pass criteria

Each matrix cell must validate:

1. schema and keyword parity
2. measure type/reference/value parity
3. coordinate-system record parity
4. world/pixel conversion parity for deterministic sample points
5. numeric tolerance conformance from `docs/phase8/tolerances.md`
6. reference-data provenance and behavior conformance from
   `docs/phase8/reference_data_policy.md`

Diagnostic dumps are allowed, but pass/fail is semantic.

## Mandatory review packet artifacts per wave

For each `P8-WX`, commit:

1. `docs/phase8/review/P8-WX/summary.md`
2. `docs/phase8/review/P8-WX/files_changed.txt`
3. `docs/phase8/review/P8-WX/check_results.txt`
4. `docs/phase8/review/P8-WX/matrix_results.json` (or analogous structured
   file for non-matrix waves)
5. `docs/phase8/review/P8-WX/open_issues.md`
6. `docs/phase8/review/P8-WX/decisions.md`

Closure rule:

1. no wave is `Done` unless all required review artifacts exist
2. no Phase-8 completion claim unless all required matrix artifacts pass all
   2x2 cells
3. no required capability may be marked pass via skip/tolerated-failure paths
4. no completion claim if any required verifier path crashes (including
   producer self-check cells)

## Carry-forward guardrails from prior misses

Phase 8 must explicitly avoid prior miss patterns:

1. do not mark a feature complete if only metadata exists but value behavior is
   unimplemented
2. do not treat skipped required checks as pass
3. do not claim full parity without corpus-backed matrix evidence
4. keep `plan`/`exit_report`/rolling-plan status text synchronized
5. ensure wave checks validate behavior, not only command success/grep markers
6. require fresh closeout reruns on current code before declaring complete
7. reject closeout evidence when reported matrix counts do not match reruns
8. treat verifier crash bugs as blocking defects, not follow-up items

## Immediate next step

After Phase-7 final closeout is merged, execute `P8-W1` and commit:

1. `docs/phase8/api_surface_map.csv`
2. `docs/phase8/dependency_decisions.md`
3. `docs/phase8/interop_artifact_inventory.md`
4. `docs/phase8/reference_data_policy.md`
5. `tools/check_phase8.sh`
6. `tools/check_phase8_w1.sh`

## Autonomy policy (implementation)

The implementing agent should execute all waves sequentially without asking for
user input during normal progress. Stop only for:

1. external dependency installation failure that blocks compilation
2. hard incompatibility requiring explicit scope change
3. data/licensing constraints preventing required fixture creation

When blocked, document in current wave review packet and continue on independent
waves where possible.
