# Phase 9 Lint Profile Lock

Locked at: 2026-02-24 (Phase 9 kickoff)

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

| Entity | Convention | Example |
|--------|-----------|---------|
| Namespace | lower_case | `casacore_mini` |
| Class/Struct | CamelCase | `MeasurementSet` |
| Enum | CamelCase | `MsMainColumn` |
| Enum constant | lower_case | `antenna1` |
| Type alias | lower_case | `cell_value` |
| Function/Method | lower_case | `create_measurement_set` |
| Parameter | lower_case | `table_dir` |
| Variable | lower_case | `row_count` |
| Local constant | lower_case | `max_rows` |
| Member | lower_case with trailing `_` | `root_path_` |
| Global constant | CamelCase with `k` prefix | `kMaxColumns` |
| Constexpr | CamelCase with `k` prefix | `kDefaultBucketSize` |
| Macro | UPPER_CASE | `CASACORE_MINI_VERSION` |

## Known suppressions with rationale

| Check | Suppression pattern | Rationale |
|-------|-------------------|-----------|
| `performance-enum-size` | `// NOLINTNEXTLINE(performance-enum-size)` on `DataType` and similar enums | Enum size must match casacore's `int32_t`-backed enums for binary compatibility |
| `bugprone-branch-clone` | Applied case-by-case | Some switch/case branches intentionally share logic for readability |
| `clang-analyzer-security.FloatLoopCounter` | Applied where float loop counters match upstream algorithm | Astronomical algorithms sometimes use float indices |
| `bugprone-easily-swappable-parameters` | Globally disabled | Too many false positives with casacore-compatible function signatures |
| `bugprone-empty-catch` | Globally disabled | Some test code uses empty catch blocks intentionally |

## Change policy

Any change to the lint profile during Phase 9 requires:
1. Explicit entry in the wave's `decisions.md` with rationale
2. Update to this document
3. Re-run of all prior wave gate scripts to confirm no regression
