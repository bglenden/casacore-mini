# P12-W8 Decisions

1. Pattern matching priority: regex is checked before glob when the token
   contains regex metacharacters (`.`, `[`, `(`, `|`, etc.). This prevents
   `3C.*` from being incorrectly treated as a glob pattern where `.` is
   literal.
2. Antenna `&&`/`&&&` mirrors the existing feed pair logic exactly. The
   `"0&"` expression is treated as incomplete (throws) rather than as a
   wildcard — use `"0&*"` for wildcard behavior explicitly.
3. Time string parsing uses the Fliegel & Van Flandern algorithm (same as
   TaQL datetime functions in W7) with JDN offset of 2400001 to produce
   midnight-aligned MJD values. Time values are in MJD seconds.
4. SPW frequency selection checks for frequency suffixes (Hz/kHz/MHz/GHz)
   before trying numeric range parsing, so that expressions like
   `"1.0~2.0GHz"` are correctly interpreted as frequency ranges rather than
   malformed integer ranges.
5. UV unit parsing strips the suffix and applies a multiplier at parse time
   (1.0 for m, 1000.0 for km). The wavelength/percentage modes are parsed
   but not fully converted, as they require additional context.
6. State bounds use the state table row index as the ID (same convention as
   all other ID-based selections), allowing `<N` and `>N` operators.
