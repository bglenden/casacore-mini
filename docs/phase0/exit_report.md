# Phase 0 Exit Report (`P0-W6`)

Date: 2026-02-23
Contract version: `0.1`

## Outcome summary

Phase 0 delivered:

- `P0-W1`: baseline pinning and fork delta map (`docs/phase0/baselines.md`)
- `P0-W2`: compatibility contract v0 (`docs/phase0/compat_contract_v0.md`)
- `P0-W3`: corpus manifest and replay assets (`docs/phase0/corpus_manifest.yaml`, `data/corpus/`)
- `P0-W4`: deterministic oracle extractor (`tools/oracle_dump.py`)
- `P0-W5`: comparator and CI gate (`tools/oracle_compare.py`, `tools/validate_corpus_manifest.py`, `tools/check_phase0.sh`, CI wiring)

`P0-W7` quality automation remains active and now includes Phase 0 checks.

## Unresolved gaps

1. Full CI interoperability against external casacore artifacts is not yet available.
- CI currently validates replay fixtures and manifest structure only.
- Full artifact verification requires a local/environment-provided casacore build tree.

2. `oracle_dump` v0 hashes TaQL textual output rather than fully typed canonical binary payloads.
- This is deterministic and useful for drift detection.
- It is weaker than the ideal type-precise hash specification in contract Section 8.

3. Persistent expression coverage is currently based on a candidate artifact (`imageexpr_candidate`) and test provenance.
- Explicit expression-encoding markers have not yet been mechanically validated.

## Top compatibility risks (ranked)

1. Storage manager edge semantics
- Risk: subtle read-path divergence in tiled manager hypercube interpretation or variable-shape edge cases.
- Impact: high (silent science-data corruption/misinterpretation risk).

2. Record and keyword typing fidelity
- Risk: nested record/value type decoding mismatch (especially measure-related metadata trees).
- Impact: high (coordinates/measures interpretation errors).

3. MeasurementSet metadata conventions
- Risk: partial parity in `TableMeasures` and MS keyword/subtable conventions despite matching table payload hashes.
- Impact: medium-high (workflow/tool interoperability regressions).

## Recommended Phase 1 scope

Phase 1 should focus narrowly on read-path persistence core and measurable parity:

1. Implement typed `Record` + `AipsIO` read-path primitives.
2. Implement table metadata/schema reader with storage-manager binding parity checks.
3. Replace text-hash data hashing with typed canonical payload hashing for scalar+array primitives.
4. Add one fully automated external-artifact CI job (self-hosted or container with casacore tools + corpus mount).
5. Reclassify expression coverage from candidate to validated (or explicitly unavailable with mitigation).

## Phase 1 entry criteria

1. `tools/oracle_dump.py` supports typed payload hashing for `bool/int/float/string/complex` scalar+array values.
2. At least one non-replay corpus artifact is checked in automated CI.
3. Any unsupported classes are explicitly listed and approved before implementation starts.
