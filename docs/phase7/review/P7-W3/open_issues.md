# P7-W3 Open Issues

## Blocking issues

None.

## Non-blocking issues

- Text path loses type precision for some numeric types (e.g. uShort parsed as
  uInt via `showtableinfo` text). The binary path preserves native types. This
  does not affect measure/coordinate metadata extraction but may surface in
  later waves if full typed keyword parity is required.

## Proposed follow-ups

- Wire `read_table_binary_metadata` into the `table_schema` active path as the
  preferred extraction method, falling back to text when binary files are
  absent.
