# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

casacore-mini is a modern C++20 reimplementation of key radio-astronomy library capabilities from casacore. The primary goal is binary-level read compatibility with persistent data formats produced by existing casacore, while replacing bespoke infrastructure with standard C++ and widely used libraries. Currently has no external C++ dependencies beyond the standard library.

## Build Commands

**Quick dev build:**
```bash
cmake -S . -B build -G Ninja -DCMAKE_CXX_COMPILER=clang++
cmake --build build
```

**Full quality build (matches CI — lint, coverage, warnings-as-errors):**
```bash
cmake -S . -B build-quality -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCASACORE_MINI_WARNINGS_AS_ERRORS=ON \
  -DCASACORE_MINI_ENABLE_CLANG_TIDY=ON \
  -DCASACORE_MINI_ENABLE_COVERAGE=ON
cmake --build build-quality
```

**Run all tests:**
```bash
ctest --test-dir build-quality --output-on-failure
```

**Run a single test:**
```bash
ctest --test-dir build-quality --output-on-failure -R record_io_test
# or directly:
./build-quality/record_io_test
```

**Check formatting (dry-run, no modifications):**
```bash
bash tools/check_format.sh
```

**Apply formatting:**
```bash
find include src tests -type f \( -name '*.hpp' -o -name '*.cpp' \) -print0 | xargs -0 clang-format -i
```

**Full CI-equivalent local quality run (format, build, lint, all phase checks, tests, coverage):**
```bash
bash tools/run_quality.sh
```

**Coverage gate (>= 70% line coverage on src/):**
```bash
bash tools/check_coverage.sh build-quality 70
```

## Architecture

- **Single flat CMakeLists.txt** at root defines all targets — no `add_subdirectory`.
- **One static library target**: `casacore_mini_core` (all `src/*.cpp`).
- **One interface target**: `casacore_mini_project_options` (carries compile flags).
- **Headers**: all in `include/casacore_mini/` (flat).
- **Sources**: all in `src/` (flat).
- **Tests**: all in `tests/` (flat). Each test is a standalone `main()` returning 0 on success — no external test framework. Tests use compile-time `CASACORE_MINI_SOURCE_DIR` to locate fixtures under `data/corpus/`.
- **Phase-gated development**: each phase has `docs/phaseN/plan.md` + `exit_report.md` and associated `tools/check_phaseN.sh` validation scripts.
- **Big-endian AipsIO decode only** (read path); little-endian host enforced at compile time via `static_assert` in `platform.hpp`.

## Naming Conventions (enforced by clang-tidy)

| Kind | Convention | Example |
|---|---|---|
| Types/classes/structs/enums | `PascalCase` | `RecordValue`, `TableSchema` |
| Functions/methods/variables/params | `snake_case` | `read_record`, `column_name` |
| Global/constexpr constants | `kPascalCase` | `kAipsIoMagic` |
| Enum constants | `lower_case` | |
| Private/protected members | `snake_case_` (trailing underscore) | `storage_`, `entries_` |
| Macros | `UPPER_CASE` | |
| Namespaces | `lower_case` | `casacore_mini` |

Legacy casacore names are preserved only when required for file-format or schema interoperability.

## Key CMake Options

| Option | Default | Purpose |
|---|---|---|
| `CASACORE_MINI_WARNINGS_AS_ERRORS` | `ON` | `-Werror` |
| `CASACORE_MINI_ENABLE_CLANG_TIDY` | `OFF` | clang-tidy during build |
| `CASACORE_MINI_ENABLE_COVERAGE` | `OFF` | `--coverage` instrumentation |
| `CASACORE_MINI_ENABLE_DOXYGEN` | `ON` | Adds `doc` target |

## Formatting and Lint

- **clang-format**: LLVM base, 4-space indent, 100-column limit, `PointerAlignment: Left`.
- **clang-tidy**: `clang-analyzer-*`, `bugprone-*`, `performance-*`, select `modernize-*` and `readability-*` checks. All warnings are errors. Header filter: `^(include|src)/`.
- Compiler warnings: `-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -Wformat=2 -Wundef -Wnon-virtual-dtor`.

## Required Tools (macOS)

```bash
brew install cmake ninja llvm clang-format gcovr doxygen
```

Ensure Homebrew LLVM's `clang++` and `clang-tidy` are on `PATH`.
