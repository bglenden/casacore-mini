# P12-W8 Open Issues

1. UV wavelength mode parses the suffix but treats the value as raw meters
   since frequency context (needed for λ→m conversion) is not available at
   the evaluator level without per-row SPW lookup.
2. UV percentage mode is parsed but treated as raw (no data-range-relative
   computation).
3. Time wildcard fields (e.g., `*/01/01` for any year) are not implemented.
   Only the full YYYY/MM/DD[/HH:MM:SS] format is supported.
4. Regex patterns use `std::regex` (ECMAScript dialect), which may differ
   from casacore's POSIX extended regex behavior on edge cases.
5. SPW frequency matching uses a 1% tolerance on ref_frequency; compound
   frequency expressions (e.g., multiple ranges) are not supported.
6. Channel stride value is parsed but not stored in `channel_ranges` (only
   start/end are stored; the stride value is currently discarded).
