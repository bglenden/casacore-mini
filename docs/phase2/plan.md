# Phase 2 Plan (Kickoff)

Date: 2026-02-23

## Objective

Move from Phase 1 text-hooked schema checks toward direct compatibility-critical
read-paths:

1. start `AipsIO`-compatible read primitives
2. move table metadata extraction toward direct file-format reads
3. broaden typed payload and `Record` coverage (including array-valued types)

## Workstreams

| ID | Workstream | Status | Deliverables |
|---|---|---|---|
| `P2-W1` | `AipsIO` read primitive scaffold | Completed | `aipsio_reader` module with canonical scalar/string/complex decode + object-header decode + tests |
| `P2-W2` | Direct table metadata read path | Pending | first metadata reader that does not depend on `showtableinfo` text output |
| `P2-W3` | Typed hash expansion | Pending | complex + multidimensional array typed payload hashing in oracle tooling |
| `P2-W4` | `Record` type matrix expansion | Pending | array-valued and additional compatibility-required value types |
| `P2-W5` | Corpus and CI expansion | Pending | at least one additional non-replay artifact in automated schema/payload checks |
| `P2-W6` | Phase 2 exit review | Pending | risk update and Phase 3 scope recommendation |

## `P2-W1` scope details

Implemented now:

- canonical big-endian primitive reads (`u16/u32/u64`, signed counterparts, `f32/f64`)
- complex reads (`complex64` as two `f32`, `complex128` as two `f64`)
- TypeIO-style string reads (`uInt` length + raw bytes)
- `AipsIO` object header reads (`magic`, object length, object type, object version)
- compile-time host guard: little-endian host only (on-disk canonical big-endian decode retained)

Deliberately deferred from `P2-W1`:

- bit-packed bool-array compatibility behavior
- nested object length-accounting checks (`getstart/getend` equivalence)
- write-path semantics

## Exit criteria for `P2-W1`

1. Reader tests validate deterministic decode of primitive and object-header paths.
2. Invalid magic and truncated input are rejected with exceptions.
3. New module is wired into existing strict quality gates.
