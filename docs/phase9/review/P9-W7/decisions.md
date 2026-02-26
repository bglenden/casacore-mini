# P9-W7 Decisions

1. **Simplified expression syntax**: Subset of casacore's MSSelection syntax
   supporting ID lists, ranges (`~`), negation (`!`), baselines (`&`), glob
   patterns (`*`), and comparison operators (`>`, `<`). No TaQL support.

2. **Correlation selection is axis-only**: Correlation expressions narrow the
   correlation axis but do not filter rows. This matches casacore behavior
   where correlation selection is a data-slicing operation.

3. **State pattern matching via glob**: `*CALIBRATE*` matches OBS_MODE
   column in STATE subtable using simple glob matching rather than regex.

4. **SPW selection via DATA_DESCRIPTION indirection**: SPW IDs are resolved
   through the DATA_DESCRIPTION subtable to produce the set of DATA_DESC_IDs
   that match, then rows are filtered by DATA_DESC_ID.

5. **Channel ranges stored but not row-filtered**: Channel specifications
   (e.g., `0:0~63`) are parsed and stored in the result for downstream use
   but do not eliminate rows from the selection.

6. **AND combination**: When multiple categories are set, the result is the
   intersection of all individual category selections.
