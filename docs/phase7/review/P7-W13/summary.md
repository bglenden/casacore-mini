# P7-W13 Summary

## Scope implemented

StandardStMan (SSM) write path: SsmWriter produces SSM data files (`table.f0`)
with correct bucket layout, column offsets, and row data. Mini-produced SSM
tables are readable by casacore with cell-value parity.

## Public API changes

- New `SsmWriter` class in `include/casacore_mini/standard_stman.hpp`

## Behavioral changes

1. `interop_mini_tool write-table-dir`: now produces SSM data files alongside
   table.dat and table.info, creating a complete table directory.
2. Matrix `mini->casacore` cell for `table_dir` now passes with value-level
   parity.
3. Matrix `mini->mini` cell for `table_dir` now verifies cell values (not just
   structure).

## Deferred items

None. SSM read+write complete.
