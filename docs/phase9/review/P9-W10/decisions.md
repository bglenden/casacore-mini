# P9-W10 Decisions

1. **Concat deduplicates by NAME**: Antenna, field, and SPW rows from the
   second MS are matched to the first MS by name. Matching rows share the
   existing ID; new rows get appended with new IDs.

2. **DATA_DESCRIPTION dedup via SPW+POL pair**: DD rows are matched by their
   remapped (SPW_ID, POLARIZATION_ID) pair, not by name.

3. **State rows are always appended**: States from ms2 are appended after
   ms1's states with a sequential remap, as there's no canonical name to
   deduplicate on.

4. **Flagger uses read-modify-write SSM pattern**: The entire SSM data file
   is read, FLAG_ROW is modified for target rows, and the SSM is rewritten.
   This preserves all other scalar column values exactly.

5. **Free functions over classes**: Both `ms_concat()` and `ms_flag_rows()`
   are free functions rather than stateful classes, matching the project's
   preference for simple functional APIs.
