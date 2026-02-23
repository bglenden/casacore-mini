# Phase 1 Plan (Detailed)

Date: 2026-02-23

## Objective

Build the minimal persistence-read core for `casacore-mini`:

1. typed `Record` data model
2. standalone binary I/O functions for record payloads
3. initial table-metadata read skeleton that can be checked against Phase-0 oracle schema

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P1-W1` | Record backend spike + API freeze v0 | In progress | backend alternatives matrix, `Record` interface contract |
| `P1-W2` | Record binary I/O scaffolding | In progress | deterministic binary read/write with round-trip tests |
| `P1-W3` | Table metadata reader skeleton | Pending | table header/schema reader + oracle schema compare hook |
| `P1-W4` | Typed payload hash upgrade | Pending | replace TaQL-text hashing for supported primitive scalar/array paths |
| `P1-W5` | Phase 1 exit review | Pending | capability delta, risks, and Phase 2 scope recommendation |

## `P1-W1` backend alternatives (spike result)

Decision criteria:

- strict typed-value fidelity for casacore persistence semantics
- deterministic serialization behavior
- low bespoke surface area
- maintainability and ecosystem maturity

Alternatives considered:

| Option | Fit for strict typed scientific records | External dependency | Notes | Disposition |
|---|---|---|---|---|
| `std::variant` + `std::vector` + map/vector entries | High | None | Strong type control and deterministic behavior with small API surface | **Selected** |
| `nlohmann::json` / JSON DOM | Low-Medium | Yes | Great tooling, but weaker numeric/type fidelity and ambiguous binary/complex handling | Not selected |
| `boost::json` | Low-Medium | Yes | Similar to JSON limitations for strict scientific type parity | Not selected |
| `yaml-cpp` node model | Low | Yes | Config-oriented; typing and canonicalization too loose for persistence parity | Not selected |
| MessagePack DOM wrappers | Medium | Yes | Better binary support, but still needs custom extension conventions | Deferred |

Phase-1 implementation rule:

- Keep `Record` as a project type.
- Keep binary I/O in standalone functions over the public `Record` interface.
- Do not lock external serialization dependencies before Phase-1 hash and schema checks are complete.

## Current implementation progress

Implemented in this phase kickoff:

- `include/casacore_mini/record.hpp`
- `include/casacore_mini/record_io.hpp`
- `src/record.cpp`
- `src/record_io.cpp`
- `tests/record_io_test.cpp`

These establish the first typed record model and deterministic binary round-trip path.

## Exit criteria for Phase 1

1. `Record` and binary I/O APIs are stable enough to support table metadata reader integration.
2. At least one table-metadata reader path emits schema data comparable to oracle schema JSON.
3. Typed payload hashing replaces TaQL text-hash for supported primitive paths.
4. CI includes Phase-1 tests in existing strict quality gates.
