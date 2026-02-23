# Phase 1 Exit Report (`P1-W5`)

Date: 2026-02-23

## Outcome summary

Phase 1 delivered the minimal persistence-read core requested for this phase:

1. typed `Record` value model and standalone binary I/O
2. table schema parser skeleton for `showtableinfo` outputs
3. oracle schema hook check for replay corpus artifact
4. typed payload hashing upgrade in `oracle_dump` for supported primitive scalar/array paths

## Deliverables completed

- Record core:
  - `include/casacore_mini/record.hpp`
  - `include/casacore_mini/record_io.hpp`
  - `src/record.cpp`
  - `src/record_io.cpp`
  - `tests/record_io_test.cpp`
- Table schema skeleton:
  - `include/casacore_mini/table_schema.hpp`
  - `src/table_schema.cpp`
  - `src/table_schema_cli.cpp`
  - `tests/table_schema_test.cpp`
- Oracle hook + automation:
  - `tools/check_phase1_schema_hook.py`
  - `tools/check_phase1.sh`
  - `.github/workflows/quality.yml` step wiring
  - `tools/run_quality.sh` step wiring
- Typed payload hash upgrade:
  - `tools/oracle_dump.py` now emits `payload_hash_mode` and `typed_payload_sha256` when supported.
- Documentation retrofit:
  - concise Doxygen comments added to public headers:
    - `include/casacore_mini/version.hpp`
    - `include/casacore_mini/record.hpp`
    - `include/casacore_mini/record_io.hpp`
    - `include/casacore_mini/table_schema.hpp`

## Documentation retrofit completion (P1 cleanup)

- Doxygen generation was already wired as an optional build target in Phase 1.
- Public API intent/invariants are now documented in headers so generated HTML has usable API guidance, not only symbol listings.

## Remaining gaps

1. Typed payload hashing currently supports a constrained subset:
- scalar/array paths where values are representable as simple one-line TaQL tokens for primitive numeric/bool/string types.
- fallback to `text_v0` remains for nested/multiline arrays and unsupported data encodings.

2. Table metadata parser currently targets `showtableinfo` textual structure.
- A direct binary table metadata reader is still needed in later phases to avoid dependence on CLI text formatting.

3. `Record` binary format is project-internal scaffolding for now.
- It is not yet casacore `AipsIO`-compatible on-disk interoperability.

## Top risks entering Phase 2

1. Rich array/complex payload normalization
- Risk: incomplete typed-hash coverage for multidimensional and complex-valued data.

2. Tool-format coupling
- Risk: schema extraction depends on `showtableinfo` textual output stability.

3. `AipsIO` semantic parity
- Risk: project-local record binary path diverges from required casacore stream semantics if not replaced/bridged early.

## Recommended Phase 2 scope

1. Expand `Record` type coverage beyond the current minimal set, including array-valued types required by corpus compatibility targets.
2. Implement table read-paths for prioritized storage managers using direct file-format-aware readers.
3. Expand typed hash support to complex values and multidimensional arrays.
4. Start `AipsIO`-compatible read primitives and connect them to `Record`/table metadata paths.
5. Add at least one non-replay corpus artifact into automated schema+payload checks in CI-capable environment.
