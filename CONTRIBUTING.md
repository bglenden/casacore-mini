# Contributing

## Goal

Local development should run the same quality checks enforced by CI.

## Required tools

- `cmake` (>= 3.27)
- `ninja`
- `clang++`
- `clang-format`
- `clang-tidy`
- `gcovr`
- `python3` (`PyYAML` optional; current manifest uses JSON subset of YAML)

## Install examples

Ubuntu/Debian:

```bash
sudo apt-get update
sudo apt-get install -y cmake ninja-build clang clang-format clang-tidy gcovr python3-yaml
```

macOS (Homebrew):

```bash
brew install cmake ninja llvm clang-format gcovr
```

If you install LLVM from Homebrew, ensure `clang++` and `clang-tidy` from that install are on `PATH`.

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
5. `ctest`
6. Coverage gate (`tools/check_coverage.sh build-quality 70`)

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
