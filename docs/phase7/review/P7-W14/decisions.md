# P7-W14 Decisions

## Autonomous decisions made

1. ISM uses "value applies forward" semantics: each column entry stores a
   start_row and value. Rows between entries inherit the previous value.
2. IsmWriter sorts entries by start_row per column before serializing to
   ensure correct forward-propagation on read.

## Assumptions adopted

1. ISM bucket size is 32768 bytes (matching casacore default).
2. ISM column index stores (start_row, offset) pairs per entry.

## Tradeoffs accepted

1. ISM read/write supports scalar columns only (Double, Int, Bool). Array
   columns in ISM are rare in practice and not present in test corpus.
