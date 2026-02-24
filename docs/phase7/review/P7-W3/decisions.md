# P7-W3 Decisions

## Autonomous decisions made

- `read_table_binary_metadata` uses a directory scan for
  `column_*_keywords.bin` files rather than requiring an explicit column list.
  This keeps the API simple and self-discovering.
- An empty or nonexistent directory returns empty metadata without error,
  allowing callers to fall back to text extraction gracefully.

## Assumptions adopted

- Binary `.bin` files in fixture directories follow the naming convention
  `table_keywords.bin` and `column_<NAME>_keywords.bin` established by the
  corpus extraction tooling.
- Each `.bin` file contains exactly one AipsIO-framed Record with no trailing
  data.

## Tradeoffs accepted

- Directory iteration order is filesystem-dependent, but metadata aggregation
  is order-independent so this does not affect determinism of the output.
