# P7-W13 Decisions

## Autonomous decisions made

1. SsmWriter produces a single bucket for the test fixture (5 rows x 4 columns
   fits within 32KB). Multi-bucket SSM write is not needed for the test corpus.
2. Column data is written in column-major order matching casacore's SSM layout.

## Assumptions adopted

1. SSM bucket capacity is sufficient for all test fixtures. Production tables
   with more rows would require multi-bucket distribution.

## Tradeoffs accepted

1. Single-bucket writer is simpler but limits row count. Acceptable for
   interop verification scope.
