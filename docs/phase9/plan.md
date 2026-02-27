# Phase 9 Plan (Full MeasurementSet)

Date: 2026-02-26
Status: Complete (W1-W14 done)

## Objective

Implement full `ms` module functionality in `casacore-mini` with strict
interoperability against `casacore` for MeasurementSet structures, data, and
operations.

This phase is compatibility-first and persistence-complete:

1. full MeasurementSet tree creation/open/read/write/update behavior
2. full subtable schema and typed-column access layer
3. MeasurementSet selection and operation APIs with parity-focused semantics
4. strict 2x2 interoperability evidence (`casacore <-> casacore-mini`)

## Entry condition

Phase 9 starts only after Phase 8 is complete and passing:

1. `bash tools/check_phase8.sh` passes
2. `bash tools/check_ci_build_lint_test_coverage.sh` passes on Phase-8 code
3. `docs/phase8/exit_report.md` is complete and internally consistent with
   `docs/casacore_plan.md`

## Definition of done

Phase 9 is complete only when all are true:

1. the API target surface in `Phase 9 API target surface` is implemented
2. MeasurementSet tree read/write/update parity is validated for required
   artifacts and subtables
3. subtable linkage integrity and foreign-key semantics are validated
4. row/column/cell and keyword behaviors are verified end-to-end through the
   MeasurementSet public interfaces (not only low-level table APIs)
5. selection behavior parity is validated for required selection categories
6. operation-layer behavior parity is validated for required operations
7. strict matrix checks pass for all required artifacts and required cells:
   - `casacore -> casacore`
   - `casacore -> casacore-mini`
   - `casacore-mini -> casacore`
   - `casacore-mini -> casacore-mini`
8. no required checks are skipped, downgraded, or accepted as expected-fail
9. no required verifier path crashes/segfaults
10. all Phase-9 waves have complete review packets
11. all API-affecting waves include Doxygen updates

## Data access architecture mandate

All access to table cell values — reads and writes — MUST go through the
high-level `Table` abstraction (`Table::read_scalar_cell`,
`Table::read_array_*_cell`, `Table::write_*_cell`, `TableRow`, etc.).

Storage managers (`SsmReader`, `IsmReader`, `TiledStManReader`,
`SsmWriter`, etc.) are internal implementation details of the `Table` class.
They may be specified at table creation time (via `TableCreateOptions`) to
control on-disk layout, but **no code outside of `Table` and its private
`Impl` should directly instantiate or call storage manager reader/writer
objects to get or set cell values**.

This applies to:

1. All MS-layer code (`MeasurementSet`, `MsWriter`, `MsColumns`, etc.)
2. All interop tool code (`interop_mini_tool`, `casacore_interop_tool`)
3. All test code that exercises table data access
4. The oracle conformance verifier (`verify-oracle-ms`)

Rationale: the `Table` class owns the column-to-SM dispatch logic. Bypassing
it duplicates routing logic, breaks when SM assignments change, and defeats
the abstraction that makes storage manager internals swappable. The oracle
verifier crash (Bus error in W13) was a direct consequence of hand-rolling SM
dispatch outside of `Table`.

Any existing code that directly uses SM readers for cell access must be
refactored to use `Table` APIs as part of Phase 9 closeout.

## Scope boundaries

### In scope

1. `ms/MeasurementSets` full core library surface needed for MS2
   compatibility, including:
   - `MeasurementSet`, `MSTable`, `MSTableImpl`
   - all required `MS*` subtable classes and `*Enums`/`*Columns` wrappers
   - `MSColumns`, `MSMainColumns`, `MSIter`, `MSRange`, `MSTileLayout`,
     `StokesConverter`, `MSDopplerUtil`, `MSHistoryHandler`
2. `ms/MSSel` selection stack required for MeasurementSet query parity:
   - `MSSelection`, `MSSelectionKeywords`, `MSSelector`, parser/index helpers
   - required selection categories listed in `Selection coverage contract`
3. `ms/MSOper` operation surfaces required for common MS workflows:
   - `MSMetaData`, `MSReader`, `MSSummary`, `MSDerivedValues`,
     `MSConcat`, `MSFlagger`, `MSValidIds`, `MSKeys`, `MSLister`
4. interoperability tools/matrix updates for full MeasurementSet artifacts
5. corpus expansion needed for full MeasurementSet coverage claims

### Out of scope

1. `ms/apps` command-line application parity (library behavior is in scope)
2. non-MS2 historical format conversion paths where no corpus or current
   workflow dependency exists (document explicitly if encountered)
3. performance-only optimizations not required for correctness parity
4. unrelated image/lattice functionality (Phase 10)

## Phase 9 API target surface

Use this as the implementation checklist and review reference.

| Group | Upstream module | Target classes/capabilities |
|---|---|---|
| Core MS table model | `ms/MeasurementSets` | `MeasurementSet`, `MSTable`, `MSTableImpl`, default-subtable creation, required table keywords/subtable refs |
| Subtable schemas | `ms/MeasurementSets` | `MSAntenna`, `MSDataDescription`, `MSDoppler`, `MSFeed`, `MSField`, `MSFlagCmd`, `MSFreqOffset`, `MSHistory`, `MSObservation`, `MSPointing`, `MSPolarization`, `MSProcessor`, `MSSource`, `MSSpectralWindow`, `MSState`, `MSSysCal`, `MSWeather` (+ enums) |
| Typed column layer | `ms/MeasurementSets` | `MSMainColumns`, `MSColumns`, all `MS*Columns` wrappers; read/write scalar/array cells and keywords through wrappers |
| Core utilities | `ms/MeasurementSets` | `MSIter`, `MSRange`, `StokesConverter`, `MSTileLayout`, `MSDopplerUtil`, `MSHistoryHandler` |
| Selection layer | `ms/MSSel` | `MSSelection`, `MSSelectionKeywords`, `MSSelector`, parser/index helpers for required selection categories |
| Operation layer | `ms/MSOper` | `MSMetaData`, `MSReader`, `MSSummary`, `MSDerivedValues`, `MSConcat`, `MSFlagger`, `MSValidIds`, `MSKeys`, `MSLister` |

## Selection coverage contract

Required selection categories for Phase-9 completion:

1. antenna
2. field
3. spectral window / channel
4. correlation / polarization
5. scan
6. time
7. uv-distance
8. state / observation / array

For each category, Phase 9 must provide:

1. parse success/failure behavior parity on required expressions
2. deterministic selected-row equivalence vs `casacore` on required fixtures
3. explicit malformed-expression tests

## Execution order and dependencies (strict)

Waves are sequential and executed in this order:

1. `P9-W1` API/corpus/operation contract freeze
2. `P9-W2` MeasurementSet core model and table-tree lifecycle
3. `P9-W3` subtable class + enum coverage
4. `P9-W4` typed-column wrappers (`MSColumns`, `MSMainColumns`, `MS*Columns`)
5. `P9-W5` write/update flows and persistent tree integrity
6. `P9-W6` core utilities (`MSIter`, `MSRange`, `StokesConverter`,
   `MSDopplerUtil`, `MSTileLayout`)
7. `P9-W7` selection framework foundation (`MSSelection*` core)
8. `P9-W8` full required selection-category behavior
9. `P9-W9` operation-layer read/introspection (`MSMetaData`, `MSReader`,
   `MSSummary`, `MSDerivedValues`)
10. `P9-W10` operation-layer mutation (`MSConcat`, `MSFlagger`,
    `MSValidIds`, `MSHistoryHandler`)
11. `P9-W11` strict interoperability matrix + hardening
12. `P9-W12` closeout and Phase-10 handoff
13. `P9-W13` oracle conformance gate (real MS cell-by-cell verification)
14. `P9-W14` final closeout (update exit report, reconcile status)

No user confirmation pauses between waves during normal execution.

## Required quality and documentation gates (every wave)

Mandatory commands:

1. `bash tools/check_format.sh`
2. `bash tools/check_ci_build_lint_test_coverage.sh`
3. `bash tools/check_docs.sh`
4. `bash tools/check_phase9_wX.sh` for current wave

Phase aggregate command:

1. `bash tools/check_phase9.sh`

Gate rules:

1. warnings-as-errors and clang-tidy are non-optional
2. required tests may not be downgraded to informational
3. required matrix/equivalence checks may not be skipped
4. functional claims must be validated by executable tests/scripts (not
   grep-only proxies)
5. verifier crashes are automatic wave failures
6. compatibility-facing waves must include producer self-check validity
   (`casacore->casacore`, `casacore-mini->casacore-mini`)
7. wave checks must use command exit status as the pass/fail source of truth;
   text matching/grep may only be diagnostic output

## Workstreams

| ID | Status | Scope | Required deliverables |
|---|---|---|---|
| `P9-W1` | Done | API/corpus/contract freeze | API surface map, artifact inventory, selection/operation coverage specs, check scaffolding |
| `P9-W2` | Done | MS core model | `MeasurementSet`/`MSTable` lifecycle and persistent tree wiring |
| `P9-W3` | Done | Subtable schemas | required `MS*` subtable classes and enum compatibility |
| `P9-W4` | Done | Typed column wrappers | `MSColumns`, `MSMainColumns`, all required `MS*Columns` wrappers |
| `P9-W5` | Done | Write/update integrity | create/update/delete flows and subtable-link consistency |
| `P9-W6` | Done | Utility layer | iter/range/stokes/doppler/tile/history utility behavior |
| `P9-W7` | Done | Selection foundation | core selection objects, keyword parsing, error semantics |
| `P9-W8` | Done | Selection completeness | required selection categories and row-set parity |
| `P9-W9` | Done | Read/introspection ops | MsMetaData, MsSummary |
| `P9-W10` | Done | Mutation ops | MsConcat, MsFlagger, subtable dedup/remap |
| `P9-W11` | Done | Matrix + hardening | 5 interop artifacts, malformed-input hardening |
| `P9-W12` | Done | Closeout (preliminary) | exit report (draft), status reconciliation, Phase-10 entry gate declaration |
| `P9-W13` | Done | Oracle conformance gate | cell-by-cell verification of real MS against upstream casacore oracle dump (`docs/phase9/oracle_conformance_plan.md`); verifier uses Table API only; oracle gate now passes end-to-end |
| `P9-W14` | Done | Final closeout | refreshed closeout evidence (`check_phase9`, interop matrix, CI build/lint/test/coverage), reconciled plan/exit/rolling status, and completed review packets W13/W14 |

## Wave details (implementation-ready)

### `P9-W1` API/corpus/contract freeze

Implementation tasks:

1. create API map from `casacore-original/ms` to `casacore-mini`:
   - `docs/phase9/api_surface_map.csv`
2. define required interop artifacts and semantic checks:
   - `docs/phase9/interop_artifact_inventory.md`
   - `docs/phase9/selection_coverage_contract.md`
   - `docs/phase9/operations_coverage_contract.md`
3. scaffold phase checks/matrix runner:
   - `tools/check_phase9.sh`
   - `tools/check_phase9_w1.sh`
   - `tools/interop/run_phase9_matrix.sh` (skeleton)
4. define numeric/string/timestamp tolerance policy:
   - `docs/phase9/tolerances.md`
5. define required review packet template:
   - `docs/phase9/review_packet_template.md` (seed/update as needed)
6. lock lint profile for this phase:
   - `docs/phase9/lint_profile.md` capturing active `clang-tidy` check set and
     known suppressions with rationale
   - wave checks verify lint-profile lock presence and consistency
7. add API-surface conformance check scaffolding:
   - mechanism that ties each required API-map row to concrete symbol evidence
     and at least one exercising test command

Expected touchpoints:

1. `docs/phase9/*`
2. `tools/check_phase9*.sh`
3. `tools/interop/run_phase9_matrix.sh`
4. `.clang-tidy` (read/verification context; changes require explicit decision)

Wave gate:

1. all required contracts exist and are explicit
2. API map covers all in-scope classes from this plan
3. lint profile lock exists and is internally consistent with active lint config
4. API-surface conformance mechanism is in place for later-wave enforcement
5. `bash tools/check_phase9_w1.sh` passes

### `P9-W2` MeasurementSet core model and lifecycle

Implementation tasks:

1. implement `MeasurementSet`, `MSTable`, `MSTableImpl` core lifecycle:
   - create new MS tree
   - open existing MS tree
   - required subtable reference handling
2. wire required table keywords/subtable linkage semantics
3. implement robust malformed-tree handling and errors
4. add round-trip tests for minimal and non-minimal MS trees

Expected touchpoints:

1. new MS core headers/sources
2. table directory integration points from Phase 7
3. tests under `tests/`

Wave gate:

1. `tools/check_phase9_w2.sh` passes
2. create/open/roundtrip tests pass for required core fixtures

### `P9-W3` subtable schemas and enums

Implementation tasks:

1. implement required subtable class coverage listed in API target surface
2. implement enum/value semantics and required keyword/schema constants
3. validate schema parity against `casacore` table descriptions
4. add malformed-schema guard tests

Expected touchpoints:

1. `include/casacore_mini/ms_*` / `src/ms_*` modules
2. tests and fixture generators
3. interop tool schema dump commands

Wave gate:

1. `tools/check_phase9_w3.sh` passes
2. schema parity checks pass for all required subtables

### `P9-W4` typed-column wrapper layer

Implementation tasks:

1. implement:
   - `MSColumns`
   - `MSMainColumns`
   - all required `MS*Columns` wrappers
2. expose read/write row/column/cell and keyword access via wrappers
3. verify wrappers preserve shape/type/units/measure metadata semantics
4. add wrapper-level parity tests vs direct table reads
5. enforce API-map conformance for wrapper entries:
   - each required wrapper row has symbol evidence in headers/sources
   - each required wrapper row has at least one exercising test path

Expected touchpoints:

1. MS column wrapper headers/sources
2. tests with row/column/cell keyword validations
3. interop verification helpers

Wave gate:

1. `tools/check_phase9_w4.sh` passes
2. required wrapper operations are behavior-tested, not only compile-tested
3. API-map wrapper entries are all mapped to symbol + exercising-test evidence

### `P9-W5` write/update flows and persistent integrity

Implementation tasks:

1. implement MS write/update flows:
   - add/remove/update rows
   - update related subtable references and IDs
   - preserve required keyword integrity
2. verify foreign-key and index consistency after mutations
3. verify persistence round-trip after mutation sequences
4. add corruption/partial-write guard behavior tests where supported

Expected touchpoints:

1. core MS lifecycle and column wrappers
2. tests for update scenarios
3. interop matrix artifact generators

Wave gate:

1. `tools/check_phase9_w5.sh` passes
2. mutation round-trip parity checks pass on required artifacts

### `P9-W6` utility layer (`MSIter`, `MSRange`, converters/utils)

Implementation tasks:

1. implement required utility behavior:
   - `MSIter`
   - `MSRange`
   - `StokesConverter`
   - `MSDopplerUtil`
   - `MSTileLayout`
   - `MSHistoryHandler` (library-level behavior)
2. add deterministic tests for iterator grouping/range outputs
3. validate utility outputs against `casacore` on fixed fixtures

Expected touchpoints:

1. utility headers/sources
2. tests and interop helper commands

Wave gate:

1. `tools/check_phase9_w6.sh` passes
2. utility parity vectors pass for required cases

### `P9-W7` selection foundation (`MSSelection*` core)

Implementation tasks:

1. implement core selection APIs:
   - `MSSelection`
   - `MSSelectionKeywords`
   - `MSSelector`
2. implement parse error surfaces and diagnostics for malformed expressions
3. implement deterministic keyword normalization behavior
4. add baseline tests for parse+evaluate of simple expressions

Expected touchpoints:

1. selection headers/sources under `MSSel` analog
2. tests for parse/evaluate/diagnostics
3. interop tool selection-check commands

Wave gate:

1. `tools/check_phase9_w7.sh` passes
2. parse/evaluate behaviors are validated with executable row-set checks

### `P9-W8` full required selection-category behavior

Implementation tasks:

1. implement/complete required selection categories:
   - antenna, field, spw/channel, corr/pol, scan, time, uvdist, state/obs/array
2. add deterministic row-set parity tests vs `casacore` for each category
3. add mixed-expression tests and malformed-expression negative tests
4. add performance guard tests for large-expression edge cases (correctness
   first; no perf tuning requirement)

Expected touchpoints:

1. selection parser/evaluator and index helpers
2. fixtures for selection coverage
3. matrix runner selection cases

Wave gate:

1. `tools/check_phase9_w8.sh` passes
2. required selection categories show zero required skips

### `P9-W9` read/introspection operations

Implementation tasks:

1. implement required operation-layer readers/introspection:
   - `MSMetaData`
   - `MSReader`
   - `MSSummary`
   - `MSDerivedValues`
   - `MSKeys`, `MSLister` where required by library coverage
2. validate operation outputs on fixed fixtures vs `casacore`
3. add malformed-input and missing-subtable behavior tests

Expected touchpoints:

1. operation-layer headers/sources
2. tests and interop operations commands

Wave gate:

1. `tools/check_phase9_w9.sh` passes
2. operation outputs pass parity checks on required fixtures

### `P9-W10` mutation operations

Implementation tasks:

1. implement mutation operations:
   - `MSConcat`
   - `MSFlagger`
   - `MSValidIds`
2. verify post-operation schema/data/link integrity
3. verify deterministic outputs for required concat/flagging scenarios
4. add cross-checks on affected subtables and history records

Expected touchpoints:

1. operation-layer mutation modules
2. tests for concat/flag operations
3. matrix mutation cases

Wave gate:

1. `tools/check_phase9_w10.sh` passes
2. concat/flag/valid-id parity checks pass for required scenarios

### `P9-W11` strict matrix + hardening

Implementation tasks:

1. complete strict phase matrix:
   - `tools/interop/run_phase9_matrix.sh`
2. include required artifact families and semantic checks
3. add malformed/corrupt-input hardening tests
4. add crash-focused verifier robustness checks
5. ensure matrix failure handling is strictly gating

Expected touchpoints:

1. matrix runner + interop tools
2. check scripts
3. hardening tests

Wave gate:

1. `tools/check_phase9_w11.sh` passes
2. required matrix cells pass; no required skips/expected-fails
3. zero required verifier crashes

### `P9-W13` oracle conformance gate

See `docs/phase9/oracle_conformance_plan.md` for full specification.

Implementation tasks:

1. implement `dump-oracle-ms` subcommand in `casacore_interop_tool.cpp`:
   dump structure, keywords, and every cell value of the pre-built
   `mssel_test_small_multifield_spw.ms` to a deterministic line-based text file
2. generate and commit the oracle dump to `data/corpus/oracle/`
3. implement `verify-oracle-ms` subcommand in `interop_mini_tool.cpp`:
   parse the oracle dump, open the same MS via casacore-mini, verify every
   value matches cell-by-cell.
   **CRITICAL**: the verifier MUST use `Table::open()` and the `Table` cell-read
   APIs (`read_scalar_cell`, `read_array_*_cell`) for all value access. It must
   NOT directly instantiate SM readers (`SsmReader`, `IsmReader`,
   `TiledStManReader`). If `Table` cannot read a cell type, that is a `Table`
   bug to be fixed — not a reason to bypass the abstraction.
4. refactor the existing `verify-oracle-ms` implementation to use `Table` APIs
   instead of raw SM dispatch (the current implementation hand-rolls SM routing
   and crashes on real-world MS data as a direct consequence)
5. fix any `Table`-layer bugs surfaced by the oracle verifier (e.g. missing
   indirect array read support, ISM column handling)
6. create `tools/check_oracle.sh` phase gate script
7. fix any casacore-mini reader bugs surfaced by the oracle verifier

Expected touchpoints:

1. `tools/interop/casacore_interop_tool.cpp`
2. `src/interop_mini_tool.cpp`
3. `include/casacore_mini/table.hpp` / `src/table.cpp` (Table API gaps)
4. `data/corpus/oracle/`
5. `tools/check_oracle.sh`
6. `docs/phase9/oracle_format.md`

Wave gate:

1. `tools/check_oracle.sh` passes (zero failures, skips only for allowed items)
2. oracle dump is committed and reproducible via `tools/interop/generate_oracle.sh`
3. skip allow-list matches `docs/phase9/known_differences.md` exactly
4. `verify-oracle-ms` uses only `Table` APIs for cell access (no direct SM use)

### `P9-W14` closeout and Phase-10 handoff

Implementation tasks:

1. produce `docs/phase9/exit_report.md` with:
   - objective outcomes
   - matrix tables and semantic evidence
   - known differences
   - carry-forward items (if any)
2. reconcile status across:
   - `docs/phase9/plan.md`
   - `docs/phase9/exit_report.md`
   - `docs/casacore_plan.md`
3. declare Phase-10 entry conditions
4. regenerate final evidence from fresh reruns:
   - `bash tools/check_phase9.sh build-p9-closeout`
   - `bash tools/interop/run_phase9_matrix.sh build-p9-closeout`
   - `bash tools/check_ci_build_lint_test_coverage.sh build-p9-closeout-ci`

Expected touchpoints:

1. `docs/phase9/*`
2. rolling plan status updates
3. aggregate checks

Wave gate:

1. `tools/check_phase9.sh` passes end-to-end
2. no status inconsistency remains in plan/exit/rolling docs
3. closeout evidence is freshly generated and count-consistent
4. closeout evidence includes fresh-build command outputs (no reused stale logs)

## Interoperability and evaluation contract (strict, no skip-pass)

### Matrix shape

Required for each required artifact:

1. `casacore -> casacore`
2. `casacore -> casacore-mini`
3. `casacore-mini -> casacore`
4. `casacore-mini -> casacore-mini`

### Required Phase-9 artifacts

At minimum:

1. minimal valid MeasurementSet with required subtables
2. MeasurementSet with representative main-column data (`DATA`, `FLAG`,
   `UVW`, `WEIGHT`, `SIGMA`, `TIME`, IDs)
3. MeasurementSet with broad optional subtable population (`WEATHER`, `SOURCE`,
   `POINTING`, `SYSCAL`, `FREQ_OFFSET`, `DOPPLER`)
4. concatenation scenario (multiple compatible input MS trees)
5. selection-stress MeasurementSet (multi-field, multi-spw, multi-scan,
   multi-array/obs/state)

### Semantic pass criteria

Each matrix cell must validate:

1. schema and keyword parity
2. subtable linkage/foreign-key integrity
3. row/column/cell value parity (with `docs/phase9/tolerances.md`)
4. selection row-set parity for required expressions
5. operation output parity for required operation scenarios
6. error behavior parity for required malformed inputs

Diagnostic dumps are allowed; pass/fail is semantic.

## Mandatory review packet artifacts per wave

For each `P9-WX`, commit:

1. `docs/phase9/review/P9-WX/summary.md`
2. `docs/phase9/review/P9-WX/files_changed.txt`
3. `docs/phase9/review/P9-WX/check_results.txt`
4. `docs/phase9/review/P9-WX/matrix_results.json` (or analogous structured file
   for non-matrix waves)
5. `docs/phase9/review/P9-WX/open_issues.md`
6. `docs/phase9/review/P9-WX/decisions.md`

Closure rule:

1. no wave is `Done` unless all required review artifacts exist
2. no Phase-9 completion claim unless required matrix artifacts pass all 2x2
   cells
3. no required capability may be marked pass through skip/tolerated-failure
4. no completion claim if any required verifier path crashes

## Carry-forward guardrails

Phase 9 must explicitly avoid known miss patterns:

1. do not claim completion from metadata-only compatibility when value behavior
   remains incomplete
2. do not treat skipped required checks as pass
3. do not claim parity without corpus-backed matrix evidence
4. keep `plan`/`exit_report`/rolling-plan status text synchronized
5. ensure wave checks validate behavior, not only script execution
6. require fresh closeout reruns on current code before declaring complete
7. treat verifier crash bugs as blocking defects
8. do not alter lint-check profile mid-phase without explicit plan/update entry
   and rationale
9. do not mark API surface complete unless API-map rows are backed by
   symbol-level and test-level evidence
10. keep wave checks exit-status-driven (no grep-only pass/fail logic)
11. never access table cell data via raw SM readers/writers outside of `Table`;
    all cell access must go through `Table` APIs (see `Data access architecture
    mandate` above)

## Immediate next step

After Phase 8 completion is merged, execute `P9-W1` and commit:

1. `docs/phase9/api_surface_map.csv`
2. `docs/phase9/interop_artifact_inventory.md`
3. `docs/phase9/selection_coverage_contract.md`
4. `docs/phase9/operations_coverage_contract.md`
5. `docs/phase9/tolerances.md`
6. `docs/phase9/lint_profile.md`
7. `docs/phase9/review_packet_template.md` (validate/update)
8. `tools/check_phase9.sh`
9. `tools/check_phase9_w1.sh`
10. `tools/interop/run_phase9_matrix.sh`

## Autonomy policy (implementation)

The implementing agent should execute all waves sequentially without waiting
for user input during normal flow. Stop only for:

1. external dependency installation failure blocking compilation
2. hard incompatibility requiring explicit scope change
3. data/licensing constraints preventing required fixture creation

When blocked, document in current wave review packet and continue independent
work where possible.
