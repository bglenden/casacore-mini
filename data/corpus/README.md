# Phase 0 Corpus Assets

This directory contains Phase 0 corpus assets used by `docs/phase0/corpus_manifest.yaml`.

## Layout

- `fixtures/`: repo-local replay fixtures used for deterministic CI checks.

## Why replay fixtures exist

Most Phase 0 corpus artifacts are external casacore-generated tables/images/MS trees that are not vendored in this repository.

CI runners do not have a full casacore build tree by default, so we keep a small replay fixture in-repo to enforce:

- manifest integrity checks
- deterministic oracle JSON generation checks
- comparator behavior checks

These checks validate tooling determinism; full interoperability checks against external artifacts run in developer environments with a local casacore build.
