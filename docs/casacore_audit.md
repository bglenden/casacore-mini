# casacore-mini audit and initial plan

Date: 2026-02-23

## Baseline and method
- Source tree audited: `/Users/brianglendenning/SoftwareProjects/casacore/main`
- Source commit: `b2a8f4c4c` (working tree is locally modified)
- Inputs used:
  - Build graph from `CMakeLists.txt` and per-module `CMakeLists.txt`
  - Module descriptions/dependencies from `mainpage.dox`
  - Header and class-declaration inventory (see `docs/module_inventory.csv`)
- Goal lens: modern C++ with minimal bespoke infrastructure while preserving interoperability with persistent data created by casacore.

## Disposition legend
- `KEEP`: keep capability and API surface (or a strict compatible subset), modernize internals.
- `THIN`: keep mostly as a thin adapter around a standard or widely used external library.
- `REPLACE`: replace bespoke implementation with std/third-party.
- `DEFER`: not in first implementation wave.
- `DROP`: out of scope for casacore-mini.

## Module audit (first pass)

| Module | Size (approx LoC / class decl lines) | Representative classes | Persistence interoperability impact | Proposed disposition |
|---|---:|---|---|---|
| `casa` | `172,073 / 580` | `Array`, `IPosition`, `Block`, `Record`, `AipsIO`, `ByteIO`, `Quantum`, `Unit` | **Critical** (`Record`, `AipsIO`, low-level I/O conventions feed table formats) | `SPLIT`: `KEEP` (`IO`, `Record`, `Quanta`), `REPLACE` (`Arrays`, `Containers`, `OS`, `Logging`, `Inputs`, `Json`) |
| `tables` | `214,844 / 1,044` | `Table`, `TableDesc`, `TableColumn`, `StandardStMan`, `IncrementalStMan`, `TiledDataStMan`, `TableExprNode` | **Critical** (primary on-disk table format and storage managers) | `KEEP` (core), with staged support for storage-manager families |
| `scimath` | `97,933 / 273` | `LSQFit`, `FFTServer`, `GaussianBeam`, `StatisticsAlgorithm` | Medium (used by `measures`, `tables`, `lattices`) | `SPLIT`: `KEEP` minimal required subset, `REPLACE` generic numerics with Eigen/BLAS/FFTW wrappers |
| `scimath_f` | `24,087 / 0` | Fortran kernels (`fwproj`, `hclean`, `fgridft`) | Low for table interop, medium for algorithm parity | `DEFER` then `REPLACE` with C++/library equivalents where needed |
| `measures` | `50,930 / 273` | `Measure`, `MeasFrame`, `MDirection`, `MEpoch`, `MFrequency`, `TableMeasDesc` | High (measure metadata stored in table keywords/columns) | `KEEP` |
| `meas` | `9,296 / 34` | `DirectionUDF`, `EpochUDF`, `MeasEngine` | Low for raw file interop; medium for TaQL feature parity | `DEFER` (after table + measures core) |
| `coordinates` | `37,010 / 65` | `CoordinateSystem`, `DirectionCoordinate`, `SpectralCoordinate`, `Projection` | High for image WCS metadata interpretation | `KEEP` + modernize internals |
| `lattices` | `87,842 / 240` | `Lattice`, `PagedArray`, `LatticeExpr`, `LCRegion` | Medium-high (image operations and masking stack) | `SPLIT`: `KEEP` domain abstractions, `REPLACE` bespoke array/expression internals |
| `images` | `54,574 / 217` | `PagedImage`, `FITSImage`, `ImageInterface`, `ImageRegion` | High (image persistence and WCS/regions) | `KEEP` core image persistence/coords; parser-heavy expression features can be staged |
| `ms` | `75,878 / 274` | `MeasurementSet`, `MSColumns`, `MSSelection`, `MSSummary` | High (MS schema on top of tables/measures) | `KEEP` schema/core I/O; `DEFER` advanced selection/ops initially |
| `derivedmscal` | `3,457 / 12` | `DerivedMSCal`, `UDFMSCal` | Low for file interop | `DEFER` |
| `msfits` | `22,872 / 105` | `MSFitsInput`, `MSFitsOutput`, `FITSIDItoMS1` | Medium (conversion path, not core table format) | `THIN/DEFER` (use cfitsio + minimal adapters) |
| `fits` | `29,305 / 93` | `FITS`, `FitsInput`, `FitsOutput`, `BinaryTable` | Medium (external FITS interoperability, coordinate dependencies) | `THIN` wrapper over `cfitsio` |
| `mirlib` | `13,661 / 0` | `miriad.h` API wrappers | Low unless MIRIAD support required | `DROP` for initial scope |
| `python3` | `0 / 0` (after local cleanup) | N/A | None for core C++ interop | `DEFER`; if needed later, use `pybind11` |

## Submodule/class-family disposition

### `casa`
| Submodule | Representative classes | Disposition | Preferred replacement / notes |
|---|---|---|---|
| `casa/Arrays` | `Array`, `Matrix`, `Vector`, `IPosition`, `Slicer` | `REPLACE` | C++ `std::mdspan`-style API + backend (`Eigen` or `xtensor`) and `std::array/std::vector` shapes |
| `casa/Containers` | `Block`, `PtrBlock`, allocators | `REPLACE` | `std::vector`, `std::pmr`, `std::span`, `std::unique_ptr/shared_ptr` |
| `casa/Utilities` | `CountedPtr`, `Regex`, `Sort`, `BitVector` | Mostly `REPLACE` | std smart pointers, `std::regex`/RE2 as needed, std algorithms |
| `casa/OS` | `File`, `Directory`, `Path`, `Time` | Mostly `REPLACE` | `std::filesystem`, `std::chrono`, platform shims only where required |
| `casa/Logging` | `LogIO`, `LogSink` | `REPLACE` | `spdlog` + `fmt` |
| `casa/Json` | `JsonParser`, `JsonValue` | `REPLACE` | `nlohmann/json` or `simdjson` |
| `casa/Inputs` | `Input`, `Param` | `DROP/REPLACE` | `CLI11`/`cxxopts` |
| `casa/Quanta` | `Quantum`, `Unit`, `MV*` | `KEEP` | Keep semantics for measures integration |
| `casa/IO` | `AipsIO`, `ByteIO`, `CanonicalIO`, bucket/mmap/file classes | `KEEP` (compatibility-critical) | May hide behind modern RAII wrappers, but wire format must match |
| `casa/Containers` (`Record*`) | `Record`, `RecordDesc`, `ValueHolder` | `KEEP` (compatibility-critical) | Re-implement internals with `std::variant` but keep serialized behavior |

### `tables`
| Submodule | Representative classes | Disposition | Notes |
|---|---|---|---|
| `tables/Tables` | `Table`, `TableDesc`, `TableColumn`, `TableRow`, `SetupNewTable` | `KEEP` | Core interoperability contract |
| `tables/DataMan` | `DataManager`, `StandardStMan`, `IncrementalStMan`, `Tiled*StMan`, `MemoryStMan` | `KEEP` staged | Prioritize managers seen most in existing datasets |
| `tables/TaQL` | `TableExprNode`, parser/AST classes | `KEEP` language, staged implementation | Keep grammar compatibility; parser backend can remain flex/bison or be replaced later |
| `tables/Dysco` | `DyscoStMan*` | `DEFER` | Add after core managers |
| `tables/AlternateMans` | `AntennaPairStMan*` | `DEFER/DROP` | Specialty manager |
| `tables/LogTables` | `TableLogSink` | `DEFER` | Non-core |

### Domain modules
| Module | Representative classes | Disposition | Notes |
|---|---|---|---|
| `measures/Measures` + `TableMeasures` | `Measure`, `MeasFrame`, `MDirection`, `MEpoch`, `TableMeasDesc`, `ScalarMeasColumn` | `KEEP` | Required for semantic interoperability of physical quantities |
| `coordinates/Coordinates` | `CoordinateSystem`, `DirectionCoordinate`, `SpectralCoordinate`, `Projection` | `KEEP` | Required for image/MS metadata interpretation |
| `images/Images` + `Regions` | `PagedImage`, `ImageInfo`, `ImageRegion`, `WCRegion` | `KEEP` staged | Keep persistence path first, expression parser features later |
| `lattices/*` | `Lattice`, `PagedArray`, `LEL*`, `LCRegion*` | `SPLIT` | Keep user-visible model, replace bespoke tensor/expression internals |
| `ms/MeasurementSets` | `MeasurementSet`, `MSColumns`, `MS*` table classes | `KEEP` staged | Core MS schema and I/O first |
| `ms/MSSel`, `ms/MSOper` | selection grammars, operations/simulator | `DEFER` | Add after basic MS read/write interoperability |
| `fits/FITS` | `FITS`, `FitsInput`, `FitsOutput` | `THIN` | Keep as cfitsio adapter layer |
| `msfits/MSFits` | `MSFitsInput/Output` | `DEFER` | Conversion path after MS and FITS core are stable |
| `derivedmscal/DerivedMC`, `meas/MeasUDF` | TaQL UDF and derived columns | `DEFER` | Add when TaQL parity becomes a requirement |

## Compatibility-critical contracts (must match existing casacore)
1. `AipsIO` stream encoding and canonical endianness/type conversion behavior.
2. Table directory/file layout, lock metadata, and schema/keyword encoding (`TableDesc`, `TableRecord`, `Record`).
3. Storage-manager wire/file formats at least for common managers:
   - `StandardStMan`
   - `IncrementalStMan`
   - `TiledShapeStMan`
   - `TiledDataStMan`
   - `TiledColumnStMan`
   - `TiledCellStMan`
4. Measure metadata encoding in table keywords/columns (`TableMeasures`).
5. MeasurementSet schema conventions (`MS*` subtables and keywords).
6. Image table conventions (`PagedImage`) and coordinate metadata conventions.

## Recommended external dependencies (to reduce bespoke code)
- Containers/utility/runtime: C++20 standard library first (`std::span`, `std::filesystem`, `std::chrono`, `std::variant`, `std::pmr`).
- Logging/formatting: `spdlog` + `fmt`.
- JSON: `nlohmann/json` (or `simdjson` for perf-critical parse paths).
- Linear algebra: `Eigen` + BLAS/LAPACK/FFTW where needed.
- CLI (apps only): `CLI11`.
- Python bindings (if/when needed): `pybind11`.
- FITS/WCS: keep using `cfitsio` and `wcslib`.

## Initial implementation plan

### Phase 0: interoperability contract and corpus (mandatory first)
- Define supported casacore format versions and exact success criteria.
- Build a frozen test corpus of real casacore artifacts: generic tables, MeasurementSets, PagedImage tables, FITS round-trip fixtures.
- Add conformance harness that compares `casacore` vs `casacore-mini` reads (schema, keywords, scalar/array values).

### Phase 1: minimal compatibility core
- Implement `mini::record` (`Record`-compatible semantics) using `std::variant`.
- Implement `mini::io` with `AipsIO`-compatible read primitives and canonical numeric conversions.
- Add modern wrappers for files/mmap/locking using std + narrow platform shims.
- Deliverable: open table metadata and read simple scalar columns from existing casacore tables.

### Phase 2: tables read-path (core storage managers)
- Implement table descriptor/keyword parser and column access for the core managers listed above.
- Support row/column slicing APIs sufficient for MS and image readers.
- Defer uncommon managers (`Dysco`, `AlternateMans`, `Adios2`) to plugin phase.
- Deliverable: read representative MeasurementSet and PagedImage table trees with parity checks.

### Phase 3: measures + coordinates
- Port `Quanta` + `Measures` semantics needed by `TableMeasures`.
- Port coordinate objects needed for image and MS metadata interpretation.
- Validate conversions against casacore reference outputs for representative frame transforms.
- Deliverable: semantic parity for common frame/unit conversions used by MS/images.

### Phase 4: images and lattice abstraction
- Introduce array backend abstraction (decision point: Eigen-backed N-D adapter vs xtensor).
- Implement `PagedImage` compatibility path first; stage expression parser and advanced region operations.
- Deliverable: read/write core image metadata and pixel planes from existing casacore image tables.

### Phase 5: MS feature expansion and write-path parity
- Implement write-path for core storage managers with compatibility tests.
- Expand MS operations (`MSSel`/`MSOper`) incrementally based on real user workflows.
- Add `msfits` conversion only after MS + FITS core are stable.
- Deliverable: create new datasets readable by upstream casacore and vice versa.

### Phase 6: optional/advanced modules
- Evaluate `Dysco`, `Adios2`, `AlternateMans`, `derivedmscal`, `meas` UDFs based on demand.
- Add Python bindings if needed using `pybind11`.

## Open decisions for next iteration
1. Array backend decision (`Eigen` adapters vs `xtensor` vs `mdspan` + custom view layer).
2. TaQL compatibility target in v1 (`read-only selection subset` vs `full parser+engine`).
3. Exact storage-manager support target for first public release (core 6 only vs include `Dysco`).
4. Whether `ms` belongs in v1 or v1.5 (depends on immediate user workflows).

## Quantitative snapshot
- Module-level counts are stored in `docs/module_inventory.csv`.
- Selected include blast radius in current casacore tree:
  - `#include <casacore/casa/Arrays/IPosition.h>`: `255`
  - `#include <casacore/casa/Arrays/Array.h>`: `205`
  - `#include <casacore/tables/Tables/Table.h>`: `236`
  - `#include <casacore/casa/Containers/Block.h>`: `134`

These counts indicate that replacing array/container primitives should be done behind compatibility wrappers, not via immediate large-scale API breaks.
