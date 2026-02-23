# Phase 0 Compatibility Contract v0 (`P0-W2`)

Date: 2026-02-23
Contract version: `0.1`

## 1. Purpose
This contract defines how `casacore-mini` interoperability is evaluated against baseline `casacore` behavior for persistence-critical artifacts.

It is designed to be directly implementable by:
- `tools/oracle_dump` (`P0-W4`)
- `tools/oracle_compare` (`P0-W5`)

## 2. Baseline references

- Baselines are pinned in `docs/phase0/baselines.md`.
- Upstream compatibility reference is `casacore/casacore` at pinned commit.
- Fork deltas are additional requirements only when present in committed baseline scope.

## 3. Compatibility model

### 3.1 Compatibility levels

1. `FORMAT` (strict):
- directory/file layout presence
- table schema and storage-manager bindings
- keyword trees and types

2. `VALUE` (strict for persisted data):
- scalar and array values read from persisted artifacts
- record-valued metadata payloads

3. `BEHAVIOR` (limited in Phase 0):
- no broad API parity requirement
- only behaviors necessary to read and compare persistence surfaces

### 3.2 Phase-0 scope
In scope:
- read-path parity for persistence-critical artifacts in corpus
- deterministic extraction and comparison

Out of scope (Phase 0):
- write-path compatibility
- full TaQL execution parity
- performance targets
- thread-safety/locking-runtime behavior parity beyond persisted lock metadata representation

## 4. Artifact classes and required checks

Every artifact in `docs/phase0/corpus_manifest.yaml` must be tagged with feature classes. Required classes (or explicit documented absence):

1. generic table (scalars + keywords)
2. generic table (array columns)
3. core storage-manager representatives
4. record-valued metadata
5. `TableMeasures` metadata
6. MeasurementSet tree
7. PagedImage + coordinate metadata
8. FITS fixture(s)
9. persistent expression artifacts (if observed)

For each class, checks are applied per Sections 6-8.

## 5. Canonical oracle output contract

`oracle_dump` emits one canonical JSON document per artifact.

Top-level object fields (required):
- `contract_version`
- `producer`
  - `tool_name`
  - `tool_version`
  - `source_commit`
- `baseline`
  - `upstream_commit`
  - `fork_commit`
- `artifact`
  - `artifact_id`
  - `artifact_type`
  - `source_path`
  - `content_hash`
- `schema`
- `keywords`
- `data`
- `diagnostics`

### 5.1 Deterministic JSON rules
- UTF-8 encoding.
- Object keys sorted lexicographically.
- Arrays preserve defined contract order.
- No nondeterministic fields except explicit timestamps under `diagnostics.generated_at_utc`.
- `oracle_compare` ignores timestamp-only fields.

### 5.2 Table schema representation (`schema`)
Must include at minimum:
- table name/path (logical)
- table type (plain/reference/concat/null as applicable)
- row count
- column list in declared schema order (see 6.1)
- per column:
  - name
  - scalar/array
  - data type
  - fixed/variable shape and shape value if fixed
  - storage-manager name and manager-specific column identifier where available

### 5.3 Keyword representation (`keywords`)
Must include:
- table keywords
- per-column keywords
- full nested record tree
- explicit type tags per leaf value

### 5.4 Data representation (`data`)
For each column:
- `row_count`
- `value_mode` (`full` or `sampled`)
- `full_payload_sha256` (required unless explicitly unsupported)
- optional `samples` payload for diagnosis
- for array columns: shape statistics and sampled shape/value snippets

## 6. Canonical ordering rules

### 6.1 Column order
Canonical column order is the declared schema order from the source artifact.
Order mismatches are schema mismatches.

### 6.2 Keyword order
Canonical order is ascending lexical key order at each record/object depth.

### 6.3 Row sampling order
If sampled mode is used:
- include `0` and `row_count-1` when row_count > 0
- include a deterministic stride set:
  - stride = `max(1, floor(row_count / 1024))`
  - indices = `0, stride, 2*stride, ...`
- enforce sample cap `4096` while always retaining anchor rows (`0`, `row_count-1`)
- fill remaining slots from stride indices in ascending order
- final index list sorted ascending and deduplicated

## 7. Value equality rules

### 7.1 Strict-equality domain (Phase 0 default)
Persisted values are compared with exact equality for:
- `bool`
- signed/unsigned integers (all widths)
- strings (exact UTF-8 bytes)
- complex values (exact equality of real and imaginary components)
- floating-point persisted payloads (exact bitwise equality)

### 7.2 Floating special values
For persisted float comparisons:
- `+inf`/`-inf` must match exactly
- `NaN` must match bit pattern exactly in Phase 0 strict mode

### 7.3 Tolerant-equality domain
No tolerant numeric mode is enabled by default in Phase 0.
If a corpus class requires derived/computed comparisons later, tolerance policy must be explicitly added as `contract_version 0.2+`.

## 8. Hashing and payload canonicalization

`full_payload_sha256` is computed over canonical logical values, not raw file bytes.

Canonical encoding for hashing:
- bool: `uint8` (`0`/`1`)
- signed/unsigned ints: little-endian fixed width
- float32/float64: IEEE-754 raw bits little-endian
- complex: encoded as `(real, imag)` back-to-back
- string: `uint32_le(byte_length)` + raw UTF-8 bytes
- arrays: flattened with last dimension varying fastest (C-order)

If a value type cannot be encoded by this scheme in v0:
- mark artifact result as `UNSUPPORTED_TYPE_V0`
- include diagnostic type path
- do not silently skip

## 9. Comparison result model

`oracle_compare` result categories:
- `PASS`: all required checks passed
- `FAIL_SCHEMA`: schema mismatch
- `FAIL_KEYWORDS`: keyword tree/type/value mismatch
- `FAIL_DATA`: persisted value mismatch
- `FAIL_STORAGE_BINDING`: storage-manager binding mismatch
- `UNSUPPORTED`: artifact/type outside implemented comparator support
- `ERROR`: tool/runtime failure

Exit status policy:
- non-zero for any `FAIL_*` or `ERROR`
- `UNSUPPORTED` is non-zero unless explicitly allowlisted for that run

## 10. Severity policy

- `Critical`:
  - schema/keyword/data/storage-binding mismatches
- `Major`:
  - deterministic-output violations
- `Minor`:
  - diagnostic-field drift not affecting normalized compare

## 11. Traceability requirements

Every compare result must include:
- `artifact_id`
- baseline commit IDs
- comparator version
- mismatch path(s) using JSON pointer notation
- first differing value pair (or hash mismatch details)

## 12. Change control

- Contract changes require bumping `contract_version`.
- Backward-incompatible compare rule changes require minor version increment (`0.1` -> `0.2`).
- `casacore_plan.md` and this file must stay aligned.

## 13. Phase-0 acceptance hooks for `P0-W2`

`P0-W2` is complete when:
1. This document is approved.
2. `P0-W4` oracle output fields map 1:1 to Section 5.
3. `P0-W5` comparator statuses and exit behavior match Sections 9-11.
4. Any unsupported corpus types are explicitly enumerated in `docs/phase0/exit_report.md`.
