# P7-W3 Summary

## Scope implemented

Active binary metadata integration: added `read_table_binary_metadata()` as a
new public API that reads measure/coordinate metadata directly from binary
`.bin` keyword files in a table fixture directory, bypassing the text-based
`showtableinfo.txt` parsing path.

Deliverables:

- `include/casacore_mini/table_binary_schema.hpp`: public header declaring
  `read_table_binary_metadata()`.
- `src/table_binary_schema.cpp`: implementation that scans a fixture directory
  for `table_keywords.bin` and `column_<NAME>_keywords.bin` files, decodes
  each via `read_aipsio_record`, and aggregates metadata via
  `extract_record_metadata`.
- `tests/table_binary_schema_test.cpp`: parity tests comparing binary-path
  output against text-path output for logtable, ms_tree, and pagedimage
  fixtures, plus an empty-directory edge case.
- `tools/check_phase7_w3.sh`: wave gate script wired into CI via
  `tools/check_ci_build_lint_test_coverage.sh`.
- CMakeLists.txt updated with new source, test executable, and test
  registration.

## Public API changes

- Added `casacore_mini::read_table_binary_metadata(std::string_view fixture_dir)`
  in `table_binary_schema.hpp`.

## Behavioral changes

- No existing behavior modified. The new function provides an alternative
  extraction path; the text-based path remains unchanged.

## Deferred items (if any)

None.
