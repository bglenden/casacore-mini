# P9-W13 Summary: Oracle Conformance Gate

## Deliverables

1. Real-MS oracle verification path in `interop_mini_tool` (`verify-oracle-ms`) is exercised through the `Table` API-only cell access path.
2. `check_oracle.sh` runtime gate verifies casacore-mini against committed upstream oracle dump for `mssel_test_small_multifield_spw.ms`.
3. Blocking reader defects found by oracle were fixed:
   - Tiled bool tile-byte/offset/read handling.
   - ISM header/index/value endianness parsing and bounds checks.

## Outcome

- Oracle conformance gate now passes end-to-end with zero mismatches.
