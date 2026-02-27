# casacore-mini rolling plan (Phase 0 detailed)

Date: 2026-02-23

## 1. Baseline and policy

### 1.1 Source baselines
- Compatibility reference baseline: upstream `casacore/casacore` (`upstream/master` at `dede86795`, fetched 2026-02-23).
- Local working baseline used for this inventory: `bglenden/casacore` (`master` at `e8b5faf81`), currently `24` commits ahead of upstream.
- Reference source checkout: `casacore-original/` (shallow clone of upstream, `.gitignore`'d). Used for reverse-engineering storage-manager binary formats during Phase 7 recovery (W12–W16). Key paths:
  - `casacore-original/tables/DataMan/SSM*.cc` — StandardStMan implementation
  - `casacore-original/tables/DataMan/ISM*.cc` — IncrementalStMan implementation
  - `casacore-original/tables/DataMan/TSM*.cc`, `Tiled*.cc` — Tiled manager implementations
  - `casacore-original/tables/DataMan/Bucket*.cc` — Bucket cache/file infrastructure
- Practical rule: compatibility decisions are anchored to upstream behavior; local fork deltas are treated as additional requirements to catalog in Phase 0.

### 1.2 Language/toolchain target (as of February 2026)
- Required language standard: `C++20`.
- Required compilers for CI support:
  - GCC `>= 13`
  - LLVM Clang `>= 17`
  - Apple Clang `>= 16`
- Build system target: CMake `>= 3.27`.

### 1.3 Modernization policy
- Preserve persistent interoperability semantics, not historical internal architecture.
- Public API/source compatibility with historical casacore is not a primary goal.
- Naming conventions, exception model, threading model, and ownership style are free to change to idiomatic modern C++.
- Code style and naming follow `casacore-mini` conventions, not legacy casacore conventions.
  - Types/classes: `PascalCase` (e.g. `RecordView`)
  - Functions/methods/variables: `snake_case` (e.g. `read_record`, `column_name`)
  - Constants: `kPascalCase` (e.g. `kMaxSamples`)

### 1.4 Engineering quality gates (enforced from day 0)
- Formatting: `clang-format` is mandatory in CI.
- Warnings: strict warning set with warnings-as-errors in CI (`-Werror` / `/WX`).
- Lint: `clang-tidy` is mandatory in CI.
- Tests: `ctest` must pass in CI.
- Coverage: `src/` line coverage gate is mandatory in CI (initial threshold `>= 70%`, to be ratcheted upward over time).
- Documentation: every phase that introduces or changes public C++ API must update Doxygen comments for those APIs, and CI must validate docs generation.

## 2. Disposition legend
- `KEEP-CAPABILITY`: preserve capability and persistence semantics; implementation and API may change.
- `THIN`: preserve capability via a thin adapter over a widely used library.
- `REPLACE`: substitute bespoke infrastructure with std/widely used libraries.
- `DEFER`: planned but not in immediate phase.
- `DROP`: out of scope for initial product direction.

## 3. Module audit (first pass)

| Module | Size (approx LoC / class decl lines) | Persistence interoperability impact | Disposition |
|---|---:|---|---|
| `casa` | `172,073 / 580` | Critical (`AipsIO`, `Record`, low-level I/O behavior under table formats) | `SPLIT`: `KEEP-CAPABILITY` (`IO`, `Record`, `Quanta`), `REPLACE` (`Arrays`, `Containers`, `OS`, `Logging`, `Inputs`, `Json`) |
| `tables` | `214,844 / 1,044` | Critical (table storage formats and storage managers) | `KEEP-CAPABILITY` (staged by storage-manager family) |
| `scimath` | `97,933 / 273` | Medium (dependency for measures/tables/lattices) | `KEEP-CAPABILITY` with implementations primarily built on existing libraries |
| `scimath_f` | `24,087 / 0` | Medium for algorithm/perf parity in radio astronomy workflows | `KEEP-CAPABILITY` but `DEFER` implementation timing; plan C++ replacements benchmarked against legacy Fortran |
| `measures` | `50,930 / 273` | High (measure metadata in table keywords/columns) | `KEEP-CAPABILITY` |
| `meas` | `9,296 / 34` | Medium (TaQL UDF parity) | `DEFER` but explicitly in roadmap |
| `coordinates` | `37,010 / 65` | High (WCS and metadata interpretation) | `KEEP-CAPABILITY` |
| `lattices` | `87,842 / 240` | Medium-high (image processing stack; expressions may be persistent in practice) | `SPLIT`: keep domain model, replace bespoke tensor/expression internals |
| `images` | `54,574 / 217` | High (image persistence and WCS/regions) | `KEEP-CAPABILITY` staged |
| `ms` | `75,878 / 274` | High (MS schema semantics over tables/measures) | `KEEP-CAPABILITY` staged; advanced selection/ops later |
| `derivedmscal` | `3,457 / 12` | Low for raw file interop | `DEFER` |
| `msfits` | `22,872 / 105` | Medium (conversion path, not core storage format) | `DEFER` |
| `fits` | `29,305 / 93` | Medium (external FITS interop) | `THIN` or direct `cfitsio` usage |
| `mirlib` | `13,661 / 0` | Low unless MIRIAD explicitly required | `DROP` initially |
| `python3` | `0 / 0` in current local tree | None for core C++ interoperability | `DEFER` (`pybind11` if needed) |

Module counts are in `docs/module_inventory.csv`.

## 4. Clarifications from review comments

### 4.1 Record representation
Question: can we adopt an existing runtime typed record class instead of implementing one?

Decision:
- Keep a `casacore-mini` `Record` abstraction, but implement internals using standard building blocks (`std::variant`, `std::vector`, `std::string`, `std::unordered_map`/flat-map).
- Reason: we must match casacore `Record` persistence semantics and type system details tightly; there is no drop-in external library that directly matches those on-disk semantics.
- Action: Phase 0 defines exact `Record` type/value contract; Phase 1 implements it.

### 4.2 scimath/scimath_f expectation
- Agreed: retained functionality should be re-implemented largely on top of existing numerical libraries wherever possible.
- `scimath_f` remains in-scope capability-wise, but implementation is intentionally deferred until core persistence compatibility is established.
- A benchmark gate will compare C++ replacements against legacy Fortran paths before declaring parity.

### 4.3 FITS layer
- Agreed: no requirement for a custom C++ FITS class stack if direct `cfitsio` use is cleaner.
- Working stance: thin compatibility adapter where needed; direct `cfitsio` calls are acceptable.

### 4.4 Lattice expressions persistence
- Agreed: treat lattice/image expression persistence as a potential compatibility requirement.
- Phase 0 corpus must explicitly include any persistent expression cases found in existing datasets.

## 5. Compatibility-critical contracts (must match existing casacore)
1. `AipsIO` stream encoding and canonical numeric conversion behavior.
2. Table directory/file layout, lock metadata semantics, and table endianness flags.
   - casacore table storage managers can store bulk data in either big-endian
     or little-endian canonical format.
   - Compatibility scope requires read support for both on-disk table
     endianness variants.
3. Table schema and keyword encoding (`TableDesc`, `TableRecord`, `Record`).
4. Storage-manager file formats, at minimum:
   - `StandardStMan`
   - `IncrementalStMan`
   - `TiledShapeStMan`
   - `TiledDataStMan`
   - `TiledColumnStMan`
   - `TiledCellStMan`
5. Measure metadata encoding in table keywords/columns (`TableMeasures`).
6. MeasurementSet schema conventions (`MS*` tables/keywords).
7. Image table conventions (`PagedImage`) and coordinate metadata conventions.
8. If present in corpus: persistent lattice/expression encodings.

### 5.1 Table data access policy
- All access to table cell values — reads and writes — MUST go through the
  high-level `Table` abstraction (`Table::read_scalar_cell`,
  `Table::read_array_*_cell`, `Table::write_*_cell`, `TableRow`, etc.).
- Storage managers (`SsmReader`, `IsmReader`, `TiledStManReader`,
  `SsmWriter`, etc.) are internal implementation details of the `Table`
  class. They may be specified at table creation time (via
  `TableCreateOptions`) to control on-disk layout, but no code outside of
  `Table` and its private `Impl` should directly instantiate or call
  storage manager reader/writer objects to get or set cell values.
- This applies to all library code, interop tools, test code, and verifiers.
- If `Table` cannot read a cell type, that is a `Table` bug to be fixed —
  not a reason to bypass the abstraction.

### 5.2 Shape/IPosition boundary policy
- Public in-memory shape/extents APIs use unsigned dimensions:
  `std::vector<std::uint64_t>`.
- AipsIO/table wire parsing and encoding use an internal signed wire type:
  `wire_i_position` (`std::vector<std::int64_t>`).
- Conversion occurs at API boundaries. This preserves compatibility with
  historical signed wire semantics (including sentinel-style negative values)
  while keeping public array shapes explicit and non-negative.

## 6. Recommended dependency posture
- Prefer C++20 stdlib first (`std::span`, `std::filesystem`, `std::chrono`, `std::variant`, `std::pmr`).
- Numerics: `Eigen` + BLAS/LAPACK/FFTW where appropriate.
- Logging/formatting: `spdlog` + `fmt`.
- JSON/config tooling: `nlohmann/json` (or `simdjson` for heavy parse paths).
- CLI for utilities: `CLI11`.
- FITS/WCS: `cfitsio`, `wcslib`.
- Python (if needed later): `pybind11`.

## 7. Rolling-phase plan

Plan precision policy:
- Phase 0 is fully specified below.
- Phases 1-6 are historical and remain summarized at roadmap level.
- Phases 7-11 have explicit scope definitions and are refined at wave
  boundaries without changing their core objective boundaries unless approved.

### Phase 0 (detailed): interoperability contract, corpus, and oracle harness

#### 0.1 Objectives
- Pin and document compatibility targets.
- Build a reproducible corpus that covers compatibility-critical persistence surfaces.
- Produce deterministic oracle outputs from upstream casacore that all future phases can test against.
- Put automated engineering quality gates in place before substantial implementation starts.
- Define explicit Phase 1 entry criteria.

#### 0.2 Workstreams and deliverables

| ID | Workstream | Deliverables |
|---|---|---|
| `P0-W1` | Baseline pinning and delta mapping | `docs/phase0/baselines.md` with upstream commit, local fork commit, and module-level delta summary |
| `P0-W2` | Compatibility contract v0 | `docs/phase0/compat_contract_v0.md` defining exact parity checks (schema, keywords, values, ordering/tolerances) |
| `P0-W3` | Corpus assembly | `data/corpus/` plus `docs/phase0/corpus_manifest.yaml` (IDs, provenance, checksums, feature tags, licenses) |
| `P0-W4` | Oracle extractor | `tools/oracle_dump/` executable that emits canonical JSON from upstream casacore reads |
| `P0-W5` | Comparator and CI gate | `tools/oracle_compare/` and CI job validating reproducibility + manifest completeness |
| `P0-W6` | Phase 1 readiness report | `docs/phase0/exit_report.md` with unresolved gaps and recommended Phase 1 scope |
| `P0-W7` | Engineering quality automation | CI quality workflow + repo configs enforcing format, warnings-as-errors, lint, tests, and coverage |

#### 0.3 Corpus minimum coverage requirements
A dataset is Phase-0-valid only if manifest includes checksum, provenance, and feature tags.

Required feature coverage:
1. Generic table with scalar columns and keywords.
2. Generic table with array columns.
3. One dataset each for these storage managers when available:
   - `StandardStMan`
   - `IncrementalStMan`
   - `TiledShapeStMan`
   - `TiledDataStMan`
   - `TiledColumnStMan`
   - `TiledCellStMan`
4. Table using `Record`-valued metadata/keywords.
5. Table with `TableMeasures`-encoded metadata.
6. At least one MeasurementSet tree with core subtables.
7. At least one PagedImage table with coordinates metadata.
8. FITS fixtures used by existing workflows (read at minimum).
9. If encountered in real artifacts: persistent image/lattice expression cases.

#### 0.4 Oracle output contract (deterministic)
`oracle_dump` must emit canonical JSON with stable ordering and explicit typing:
- Schema block: table desc, column definitions, storage-manager bindings.
- Keywords block: table and column keywords with explicit value types.
- Data block:
  - Scalar columns: full values for small tables; sampled deterministic rows for large tables.
  - Array columns: shape, dtype, deterministic hash of payload, and optional sampled slices.
- Metadata block: source path, artifact ID, tool version, timestamp, and content hash.

#### 0.5 Phase 0 acceptance criteria (exit gate)
1. `P0-W1..P0-W7` deliverables exist and are reviewed.
2. Corpus manifest covers all required feature categories in Section 0.3 or explicitly documents unavailable categories with mitigation.
3. Oracle output is deterministic across repeated runs on the same artifact.
4. Comparator passes on baseline self-checks (upstream vs upstream, no spurious diffs).
5. CI enforces manifest integrity and oracle determinism checks.
6. CI enforces quality gates for format, warnings-as-errors, lint, test pass, docs generation, and `src/` line coverage `>= 70%`.
7. `docs/phase0/exit_report.md` names top 3 compatibility risks and a recommended Phase 1 scope.

### Phase 1+ (rolling roadmap)
- Phase 1: minimal persistence core (`Record`, `AipsIO`, table metadata read path).
- Phase 2: table read-path for prioritized storage managers plus `Record` type-matrix expansion (including array-valued types) driven by corpus compatibility needs.
- Phase 3: measures + coordinates parity for common workflows.
- Phase 4: image/lattice core capabilities.
- Phase 5: write-path parity and expanded MS operations.
- Phase 6: integrated binary keyword/record interoperability and metadata-first table write bootstrap.
- Phase 7 (complete 2026-02-24): full Tables implementation and
  interoperability, including metadata and cell-level data read/write across
  the required storage-manager set. All 6 required storage managers
  (StandardStMan, IncrementalStMan, TiledColumnStMan, TiledCellStMan,
  TiledShapeStMan, TiledDataStMan) implemented with strict 2x2 matrix
  interoperability. See `docs/phase7/exit_report.md`.
- Phase 8 (complete 2026-02-24): full Measures + Coordinates +
  CoordinateSystems implementation. All 9 measure types with frame-aware
  conversions (ERFA), 6 coordinate types with WCSLIB projections,
  CoordinateSystem composition, TableMeasures column integration, and
  utility layer (CoordinateUtil, FITSCoordinateUtil, GaussianConvert).
  24/24 interop matrix cells pass. 43 unit tests, 3 hardening test suites.
  See `docs/phase8/exit_report.md`.
- Phase 9 (complete 2026-02-24): full MeasurementSet implementation.
  MS lifecycle, 17 subtable schemas, typed column wrappers, write/update,
  utility layer (MsIter, StokesConverter, MsDopplerUtil), 8-category
  selection API, MsMetaData/MsSummary, MsConcat/MsFlagger operations.
  13/20 interop matrix cells pass (self-roundtrips + most casacore→mini;
  mini→casacore blocked by table format gap). 57 unit tests, 1 hardening
  test suite. See `docs/phase9/exit_report.md`.
- Phase 10 (complete): full Lattices + Images implementation, including LEL
  compatibility, region/mask persistence, 20/20 interop matrix, 78 tests.
  See `docs/phase10/exit_report.md`.
- Phase 11 (pending, planned terminal phase): remaining capabilities closure;
  first wave is a full missing-capabilities audit with explicit
  include/exclude decisions, followed by implementation of accepted remainder.
  Phase 11 also includes a full storage-manager fidelity audit to identify
  and remove any simplified/heuristic behavior versus upstream casacore.
  Includes: integrate ISM and TSM writers into `Table::create()` so that
  casacore-mini can produce tables with all 6 required storage managers,
  not only StandardStMan.
  Detailed wave plan: `docs/phase11/plan.md`.

Phase-1 detailed execution tracking lives in `docs/phase1/plan.md`.
Phase-1 completion summary lives in `docs/phase1/exit_report.md`.
Phase-2 detailed execution tracking lives in `docs/phase2/plan.md`.
Phase-2 completion summary lives in `docs/phase2/exit_report.md`.
Phase-3 detailed execution tracking lives in `docs/phase3/plan.md`.
Phase-3 completion summary lives in `docs/phase3/exit_report.md`.
Phase-4 detailed execution tracking lives in `docs/phase4/plan.md`.
Phase-4 completion summary lives in `docs/phase4/exit_report.md`.
Phase-5 detailed execution tracking lives in `docs/phase5/plan.md`.
Phase-5 completion summary lives in `docs/phase5/exit_report.md`.
Phase-6 detailed execution tracking lives in `docs/phase6/plan.md`.
Phase-7 detailed execution tracking lives in `docs/phase7/plan.md`.
Phase-8 detailed execution tracking lives in `docs/phase8/plan.md`.
Phase-9 detailed execution tracking lives in `docs/phase9/plan.md`.
Phase-10 detailed execution tracking lives in `docs/phase10/plan.md`.
Phase-11 detailed execution tracking lives in `docs/phase11/plan.md`.

### Mandatory structure for all future phase plans

Every new `docs/phaseN/plan.md` (N >= 7) must be implementation-ready for a
delegate AI agent and review-efficient for a separate reviewer.

Minimum required sections:

1. objective + explicit definition of done
2. scope boundaries (`In scope` / `Out of scope`)
3. execution order/dependencies between waves
4. required quality/documentation gate commands
5. workstream table with:
   - wave ID
   - status (`Pending/Done/Deferred`)
   - concrete deliverables
6. per-wave implementation details containing:
   - implementation tasks
   - expected file/module touchpoints
   - wave-specific check script(s) and acceptance gates
7. interoperability/evaluation contract (if phase is compatibility-facing)
8. immediate next step
9. autonomy policy that allows complete wave execution without waiting for user
   input during normal implementation flow
10. wave-gate design rules:
   - behavior-based checks that execute real code paths
   - no grep/string-only proxies for functional claims
   - command exit status is the pass/fail source of truth
   - explicit fail-fast handling for verifier crashes/segfaults
11. closeout evidence protocol:
   - required clean reruns for final phase gates
   - use fresh build directories for closeout aggregate reruns
   - command outputs in review packets must be from current branch state
12. lint profile policy:
   - lock active lint-check profile at phase kickoff
   - lint-profile changes require explicit plan update and rationale
13. API conformance policy:
   - compatibility-facing API maps must be enforced with
     symbol-level and exercising-test evidence per required row

Mandatory review artifacts per wave:

1. `docs/phaseN/review/PN-WX/summary.md`
2. `docs/phaseN/review/PN-WX/files_changed.txt`
3. `docs/phaseN/review/PN-WX/check_results.txt`
4. `docs/phaseN/review/PN-WX/matrix_results.json` (or analogous structured
   result file for non-matrix phases)
5. `docs/phaseN/review/PN-WX/open_issues.md`
6. `docs/phaseN/review/PN-WX/decisions.md`

Closure rule:

1. no wave is marked `Done` until required review artifacts exist and are
   internally consistent with plan claims.
2. no phase is marked `Complete` unless required aggregate checks and required
   matrix checks pass on the current branch/commit.
3. `check_results.txt` and `matrix_results.json` must reflect fresh reruns from
   current code; stale/copied historical results are invalid closeout evidence.

### Phase 7 full-Tables workflow (concrete)

Phase 7 targeted complete Tables functionality and was reopened after an
incomplete first closeout. It is now complete (2026-02-24). Verification is producer/consumer matrix based,
at full table scope:

1. table surfaces covered:
   - full `table.dat` read/write (`TableDesc`, column descriptors, manager metadata)
   - table directory semantics and sidecar files for supported managers
   - table and column keywords/records
   - scalar/array cell and row-level data operations in supported managers
2. producers:
   - `casacore` creates reference table artifacts
   - `casacore-mini` creates equivalent table artifacts
3. consumers:
   - both toolchains must open, interpret, and semantically validate both
     producer outputs
4. matrix (required):
   - casacore -> casacore
   - casacore -> casacore-mini
   - casacore-mini -> casacore
   - casacore-mini -> casacore-mini
5. validation rules:
   - semantic structure/value checks are the primary pass/fail gate
   - canonical dumps are diagnostic artifacts on failure only
   - no writer is considered compatible unless both readers validate output
6. storage-manager coverage policy:
   - required set: `StandardStMan`, `IncrementalStMan`, `TiledShapeStMan`,
     `TiledDataStMan`, `TiledColumnStMan`, `TiledCellStMan`
   - no Phase-7 deferral for the required set without explicit user approval
     to change phase boundaries
7. execution entry point:
   - `tools/interop/run_phase7_matrix.sh` (to be expanded for full-table scope)

Local prerequisite for `casacore` tooling on macOS (from upstream README):

```bash
brew tap casacore/tap
brew install casacore
```

## 8. AI Implementation Review Checklist

When another AI agent contributes implementation, review at least:

1. behavior claims vs code reality:
   - docs/plan assertions match concrete implementation behavior
   - no "full fidelity/parity" claims without corpus-backed evidence
2. API character and consistency:
   - naming/style follows project conventions (`PascalCase` types, `snake_case`
     functions)
   - public API shape matches neighboring modules (ownership, error model,
     determinism guarantees)
3. safety and malformed-input handling:
   - signed/unsigned conversions are guarded
   - negative sizes, overflow risks, and deep recursion are handled explicitly
   - undefined-behavior edge cases are avoided in pointer/length operations
4. interoperability evidence quality:
   - synthetic tests are supplemented with corpus-backed fixtures for key claims
   - both success and failure/invalid-input paths are covered
5. CI and quality integration:
   - new checks are wired into `tools/run_quality.sh` and CI workflow
   - Doxygen/API docs are updated when public APIs change
6. phase accounting accuracy:
   - plan status fields (`Pending/Done/Deferred`) match implementation reality
   - deferred items are explicit and referenced in exit reports
7. review packet completeness:
   - required `docs/phaseN/review/PN-WX/*` artifacts exist for claimed-complete
     waves and contain runnable command/evidence details
8. gate robustness and evidence integrity:
   - wave gates validate behavior by execution, not textual markers
   - reported pass/fail counts match current reruns
   - compatibility matrix includes producer self-check validity
     (`casacore->casacore`, `mini->mini`) and cross-check validity
9. lint and API-surface contract discipline:
   - lint profile changes are explicit and documented, not ad-hoc
   - required API-map entries are backed by concrete symbols and
     exercising tests

## 9. Phase-Closeout Learning Loop

To ensure iterative learning across waves, each phase closeout must feed missed
items back into the rolling plan.

Required closeout actions:

1. Every `docs/phaseN/exit_report.md` must include a `Misses observed` section:
   - concrete items that were missed, overstated, or only partially done
   - whether each miss was code, API contract, tests, docs, or planning scope
2. For each miss, define a carry-forward action:
   - `fix in next phase`, `add checklist guard`, or `defer with rationale`
3. Update this rolling plan section with any new recurring miss pattern.
4. The next phase kickoff plan must explicitly list which carry-forward actions
   were absorbed.

Carry-forward guardrails (minimum):

1. avoid declaring "done" unless integrated into active paths, not only
   standalone components
2. avoid parity/fidelity claims without corpus-backed evidence
3. require malformed-input and boundary tests for all new binary parsers/writers
4. keep API docs and behavior aligned (especially throws and fidelity semantics)
5. ensure phase checks verify critical new capabilities directly
6. before marking a workstream done, cross-check plan claims against provenance
   and active integration points (not only standalone tests)
7. enforce the mandatory phase-plan structure and per-wave review artifacts for
   every new phase kickoff
8. treat any verifier crash/segfault as a phase-blocking failure
9. require closeout evidence regeneration from clean reruns before marking
   phase complete
10. forbid completion claims while any required matrix cell fails
11. lock lint profile at phase start and document any required mid-phase changes
12. require API-map row-to-symbol and row-to-test traceability for
    compatibility-facing phases
13. use fresh build directories for final aggregate reruns

## 10. Quantitative snapshot

- Module-level inventory: `docs/module_inventory.csv`.
- Current include blast radius sample from local casacore tree:
  - `#include <casacore/casa/Arrays/IPosition.h>`: `255`
  - `#include <casacore/casa/Arrays/Array.h>`: `205`
  - `#include <casacore/tables/Tables/Table.h>`: `236`
  - `#include <casacore/casa/Containers/Block.h>`: `134`

Interpretation: replace foundational array/container internals behind compatibility facades, not via early broad mechanical rewrites.

## 11. API audit (planned)

Goal: minimize the public API surface to keep the library manageable and reduce
maintenance burden.

Actions:

1. **Audit all public methods on every class.** Any method that exists solely
   to support internal cross-class communication (e.g. `has_column()` on
   `TiledStManReader`, helper parse functions) should be moved to `private`
   or `protected`.
2. **Use `friend class` declarations** where a private method must be called
   by another class (e.g. `Table` needs to query storage-manager readers).
   Follow the pattern already applied to `TiledStManReader` / `Table`.
3. **Review free functions in headers.** Functions like `parse_ssm_blob`,
   `parse_ssm_file_header`, `parse_ssm_indices` are currently in the public
   namespace. If they are only consumed by `SsmReader::open()`, move them to
   an anonymous namespace in the `.cpp` file or make them private static
   members.
4. **Check `struct` visibility.** Internal structs such as `SsmFileHeader`,
   `SsmTableDatBlob`, and `SsmIndex` may not need to be in the public header
   if they are only used inside the implementation.
5. **Produce an API surface map** (`docs/api_surface_audit.csv`) listing every
   public symbol, its current consumers, and a disposition: `keep-public`,
   `make-private`, `make-friend`, or `internalize`.
