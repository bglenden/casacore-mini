# P8-W11 Decisions

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Key decisions

1. **Explicit malformed-input guards** — All record deserialization functions
   (measure_from_record, Coordinate::from_record, CoordinateSystem::from_record)
   validate required fields and value ranges before proceeding. Invalid input
   produces descriptive exceptions (std::invalid_argument or std::runtime_error)
   with messages identifying the missing/invalid field, rather than undefined
   behavior or cryptic downstream failures.

2. **Documented precision tolerances** — Known numerical differences between
   casacore and casacore-mini are cataloged in known_differences.md with
   explicit tolerances. This includes: epoch conversions (< 1 microsecond),
   direction conversions (< 1 milliarcsecond), position transforms (< 1 mm),
   and coordinate round-trips (< 0.01 pixel). These tolerances are used as
   acceptance criteria in the interop matrix.

## Tradeoffs accepted

1. Malformed-input tests add maintenance burden as the record format evolves.
   Accepted because input validation is essential for a library that reads
   externally-produced data files.

2. IGRF model not implemented for MEarthMagnetic. Accepted because the
   simplified dipole model is sufficient for the rare use cases of this
   measure type, and full IGRF would require vendoring ~100 KB of coefficient
   tables.
