# Phase 8 Dependency Decisions

Date: 2026-02-24

## ERFA (Essential Routines for Fundamental Astronomy)

### Decision: Use ERFA for astronomical time and frame transforms

**Rationale:**
- ERFA is the BSD-licensed fork of the IAU SOFA library, used by upstream casacore
  for epoch and direction frame conversions.
- Provides validated implementations of UTC/TAI/TT/UT1 time scale conversions,
  precession/nutation matrices (IAU 2006/2000A), geodetic/geocentric position
  transforms (WGS84), and frame rotation matrices (J2000, Galactic, Ecliptic).
- Using ERFA instead of bespoke math ensures numeric parity with upstream casacore
  which also uses SOFA/ERFA internally.
- BSD license is compatible with project licensing.

**CMake integration strategy:**
1. `pkg_check_modules(ERFA erfa)` — detects a system install (meson-built, or
   distro package like `liberfa-dev`).
2. Fallback: vendored single-file build (`third_party/erfa/erfa.c` + `erfa.h`),
   compiled as a static library within the project. ERFA's `source_flattener.py`
   tool produces these two files from the full source tree.
3. `FATAL_ERROR` if neither path succeeds.

**Note:** ERFA is **not** in Homebrew. On macOS it must be built from source
using meson/ninja (see README.md) or vendored. On Linux it is available as
`liberfa-dev` (Debian/Ubuntu) or `erfa-devel` (Fedora).

**Note:** Original casacore uses SOFA (the Fortran predecessor) only optionally
for testing measures — not as a build dependency. Its internal conversion code
reimplements the same IAU algorithms. We use ERFA directly instead of
reimplementing.

**Version pin:** ERFA 2.0.1 (tested, matches recent IAU standards).

**Alternative considered:** Implementing transforms from scratch using published IAU
formulas. Rejected because: (a) high risk of subtle precision bugs, (b) makes parity
testing harder since upstream uses SOFA/ERFA-equivalent math, (c) violates the
"minimal bespoke infrastructure" principle.

## WCSLIB (World Coordinate System Library)

### Decision: Use WCSLIB for FITS/WCS projection math

**Rationale:**
- WCSLIB implements the FITS WCS standard (Papers I-VII) and is used by upstream
  casacore for `DirectionCoordinate` pixel/world transforms.
- Covers ~30 projection types (SIN, TAN, ARC, ZEA, STG, AIT, CAR, etc.) with
  validated implementations.
- Using WCSLIB ensures projection parity with casacore artifacts.
- LGPL-3.0 license is compatible with project use (dynamic linking if needed).

**CMake integration strategy:**
1. `find_package(WCSLIB)` via pkg-config (works with Homebrew `brew install wcslib`).
2. Fallback: vendor a minimal source subset (~10 C files: `wcs.c`, `cel.c`, `prj.c`,
   `spc.c`, `lin.c`, `sph.c`, `tab.c`, `wcserr.c`, `wcsutil.c`, `wcsprintf.c`)
   compiled as a static library within the project build.
3. The vendored subset avoids pulling in cfitsio or other optional WCSLIB dependencies.

**Version pin:** WCSLIB >= 7.0 (tested with 8.x on Homebrew).

**Alternative considered:** Implementing projection math directly. Rejected because:
(a) ~30 projections with edge cases is a large surface, (b) parity testing against
casacore-produced artifacts would be difficult without using the same math, (c) WCSLIB
is the de facto standard for WCS in astronomy.

## Reference Data (IERS, Leap Seconds, Ephemerides)

### Decision: Vendored snapshots with no network fetches

Detailed in `docs/phase8/reference_data_policy.md`.

## Existing Dependencies (Unchanged)

- **C++20 standard library** — containers, algorithms, filesystem, string_view, variant
- **CMake 3.27+** — build system
- **clang-format / clang-tidy** — quality enforcement
- **Doxygen** — documentation generation

## No New Dependencies Beyond ERFA and WCSLIB

Phase 8 does not require Eigen, BLAS, FFTW, or any other numeric library. The
measure conversion math is dominated by ERFA calls and simple trigonometry. The
coordinate transform math is dominated by WCSLIB calls and small matrix operations
(2x2, 3x3) implemented inline.
