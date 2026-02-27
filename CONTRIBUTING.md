# Contributing

## Goal

Local development should run the same quality checks enforced by CI.

## Naming conventions

`casacore-mini` uses modern project conventions rather than historical casacore naming.

- Types/classes: `PascalCase`
- Functions/methods/variables: `snake_case`
- Constants: `kPascalCase`

Legacy casacore names are preserved only when required for strict file-format or schema interoperability.

Naming is enforced in CI/local lint via `clang-tidy` (`readability-identifier-naming`).

## Required tools

- `cmake` (>= 3.27)
- `ninja`
- `clang++`
- `clang-format`
- `clang-tidy`
- `gcovr`
- `python3` (`PyYAML` optional; current manifest uses JSON subset of YAML)
- `doxygen`

## Install examples

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build clang clang-format clang-tidy gcovr python3-yaml doxygen
```

macOS (Homebrew):

```bash
brew install cmake ninja llvm clang-format gcovr doxygen
```

If you install LLVM from Homebrew, ensure `clang++` and `clang-tidy` from that install are on `PATH`.

## Generate API docs

```bash
cmake -S . -B build-docs -G Ninja
cmake --build build-docs --target doc
```

Generated HTML entry point:

`build-docs/docs/doxygen/html/index.html`

## Documentation standard for code phases

Any phase that introduces or changes public C++ API must include Doxygen updates
in the same phase. At minimum, document:

1. Public type/function purpose.
2. Key invariants/layout semantics (for persistence-sensitive types).
3. Error behavior (`throws` conditions).

Phase completion should not be declared until this documentation is updated.

## CI-equivalent local workflows

From the repository root, run:

```bash
bash tools/check_format.sh
bash tools/check_ci_fast_build_test.sh build-fast
```

This mirrors fast CI on push/PR (`.github/workflows/quality.yml`).

## Worktree Merge Gate (Recommended Solo Flow)

When developing in a worktree branch and fast-forwarding into `main`, run:

```bash
bash tools/preflight_worktree_merge.sh
```

This executes:

1. Host fast CI parity (`check_ci_fast_build_test.sh`)
2. Linux container fast CI parity (`check_ci_fast_build_test_linux_container.sh`, includes format check)

To validate a different worktree from the current checkout:

```bash
bash tools/preflight_worktree_merge.sh \
  --repo-root ../casacore-mini-worktrees/phase11
```

If Linux container tools are not yet installed locally:

```bash
bash tools/preflight_worktree_merge.sh --skip-linux
```

Then install container tooling and re-run without `--skip-linux` before merge.

If you also want a host-side format check (in addition to Linux parity), add:

```bash
bash tools/preflight_worktree_merge.sh --host-format
```

For full-quality parity (nightly/manual CI), run:

```bash
bash tools/run_quality.sh
```

This performs full checks:

1. `clang-format` check (`tools/check_format.sh`)
2. CI-equivalent build/lint/test/coverage checks (`tools/check_ci_build_lint_test_coverage.sh build-quality`)
3. CI-equivalent docs checks (`tools/check_docs.sh build-quality-doc`)

## CI job parity scripts

These scripts are the source of truth for CI/local parity:

- `tools/check_ci_fast_build_test.sh`: fast configure/build/ctest path used on push/PR
- `tools/check_ci_fast_build_test_linux_container.sh`: Linux container parity for fast CI (clang/libstdc++)
- `tools/check_ci_build_lint_test_coverage.sh`: phase checks + configure/build + `ctest` + coverage gate
- `tools/check_docs.sh`: docs configure + Doxygen build + output verification
- `tools/preflight_worktree_merge.sh`: single command merge gate for worktree branches

`quality.yml` and `quality-full.yml` call these scripts directly.

## Pre-push hook (recommended)

Install:

```bash
bash tools/install_git_hooks.sh
```

This creates `.git/hooks/pre-push` that runs:

```bash
bash tools/pre_push_quality.sh
```

The hook is intentionally strict to catch regressions before upload.

## Manual commands (equivalent)

```bash
bash tools/check_format.sh
bash tools/check_ci_build_lint_test_coverage.sh build-quality
bash tools/check_docs.sh build-docs
```

## Optional: apply formatting

```bash
find include src tests -type f \
  \( -name '*.h' -o -name '*.hpp' -o -name '*.hh' -o -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) \
  -print0 | xargs -0 clang-format -i
```

## Release tags

Release tags must use `vX.Y.Z` and match `project(casacore_mini VERSION X.Y.Z)` in `CMakeLists.txt`.

Local check:

```bash
bash tools/check_release_tag.sh v0.1.0
```
