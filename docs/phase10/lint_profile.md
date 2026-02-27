# Phase 10 Lint Profile Lock

Locked at: 2026-02-26 (P10-W1)

Inherited from Phase 9 without changes.

## Active clang-tidy check set

From `.clang-tidy`:

```yaml
Checks: >
  clang-analyzer-*,
  bugprone-*,
  -bugprone-empty-catch,
  -bugprone-easily-swappable-parameters,
  performance-*,
  modernize-use-nullptr,
  modernize-use-override,
  readability-implicit-bool-conversion,
  readability-identifier-naming
WarningsAsErrors: '*'
HeaderFilterRegex: '^(include|src)/'
```

## Naming conventions

Inherited from Phase 9 (see `docs/phase9/lint_profile.md`).

## Known suppressions with rationale

Inherited from Phase 9. No new suppressions anticipated for Phase 10.
If new suppressions are needed (e.g., for expression evaluator patterns),
they will be documented in the relevant wave's `decisions.md` and added here.

## Change policy

Any change to the lint profile during Phase 10 requires:
1. Explicit entry in the wave's `decisions.md` with rationale
2. Update to this document
3. Re-run of all prior wave gate scripts to confirm no regression
