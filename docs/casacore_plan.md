# casacore-mini rolling plan (Phase 0 detailed)

Date: 2026-02-23

## 1. Baseline and policy

### 1.1 Source baselines
- Compatibility reference baseline: upstream `casacore/casacore` (`upstream/master` at `dede86795`, fetched 2026-02-23).
- Local working baseline used for this inventory: `bglenden/casacore` (`master` at `e8b5faf81`), currently `24` commits ahead of upstream.
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
- Phases 1+ are intentionally coarse and will be replanned at phase boundaries.

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

### Phase 1+ (coarse, to be replanned later)
- Phase 1: minimal persistence core (`Record`, `AipsIO`, table metadata read path).
- Phase 2: table read-path for prioritized storage managers plus `Record` type-matrix expansion (including array-valued types) driven by corpus compatibility needs.
- Phase 3: measures + coordinates parity for common workflows.
- Phase 4: image/lattice core capabilities.
- Phase 5: write-path parity and expanded MS operations.
- Phase 6: optional capabilities (`Dysco`, UDFs, conversion extras, Python bindings).

Phase-1 detailed execution tracking lives in `docs/phase1/plan.md`.
Phase-1 completion summary lives in `docs/phase1/exit_report.md`.
Phase-2 detailed execution tracking lives in `docs/phase2/plan.md`.
Phase-2 completion summary lives in `docs/phase2/exit_report.md`.
Phase-3 detailed execution tracking lives in `docs/phase3/plan.md`.
Phase-3 completion summary lives in `docs/phase3/exit_report.md`.
Phase-4 detailed execution tracking lives in `docs/phase4/plan.md`.

## 8. Quantitative snapshot
- Module-level inventory: `docs/module_inventory.csv`.
- Current include blast radius sample from local casacore tree:
  - `#include <casacore/casa/Arrays/IPosition.h>`: `255`
  - `#include <casacore/casa/Arrays/Array.h>`: `205`
  - `#include <casacore/tables/Tables/Table.h>`: `236`
  - `#include <casacore/casa/Containers/Block.h>`: `134`

Interpretation: replace foundational array/container internals behind compatibility facades, not via early broad mechanical rewrites.
