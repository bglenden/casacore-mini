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

## CI-equivalent local workflow

From the repository root, run:

```bash
bash tools/run_quality.sh
```

This performs:

1. `clang-format` check (`tools/check_format.sh`)
2. Phase 0 manifest + oracle determinism checks (`tools/check_phase0.sh build-quality`)
3. CMake configure with strict flags and lint enabled
4. Build
5. Phase 1 schema hook check (`tools/check_phase1.sh build-quality`)
6. Phase 2 compatibility checks (`tools/check_phase2.sh build-quality`)
7. Phase 3 keyword/metadata checks (`tools/check_phase3.sh`)
8. Doxygen docs generation (`doc` target)
9. `ctest`
10. Coverage gate (`tools/check_coverage.sh build-quality 70`)

## Manual commands (equivalent)

```bash
bash tools/check_format.sh
bash tools/check_phase0.sh build-quality

cmake -S . -B build-quality -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=ON \
  -DCASACORE_MINI_ENABLE_COVERAGE=ON

cmake --build build-quality
bash tools/check_phase1.sh build-quality
bash tools/check_phase2.sh build-quality
bash tools/check_phase3.sh
cmake -S . -B build-docs -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=OFF \
  -DCASACORE_MINI_ENABLE_COVERAGE=OFF \
  -DCASACORE_MINI_ENABLE_DOXYGEN=ON
cmake --build build-docs --target doc
ctest --test-dir build-quality --output-on-failure
bash tools/check_coverage.sh build-quality 70
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
