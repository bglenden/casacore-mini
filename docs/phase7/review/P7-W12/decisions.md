# P7-W12 Decisions

## Autonomous decisions made

1. Implemented SsmReader as a standalone class rather than integrating into
   TableDirectory, to keep storage manager concerns separate.
2. Used a bucket-based parser that reads the SSM header, column offsets, and
   row data from contiguous bucket regions.

## Assumptions adopted

1. SSM bucket layout follows the casacore format: 512-byte file header +
   contiguous buckets at bucket_size offsets.
2. Column data within buckets is stored in column-major order with fixed-size
   cells for scalars and offset-based cells for arrays.

## Tradeoffs accepted

1. Read-only at W12. Write path deferred to keep the wave focused and
   reviewable.
