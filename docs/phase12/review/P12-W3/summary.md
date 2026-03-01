# P12-W3 Summary: TaQL DML Closure (INSERT/DELETE)

## Deliverables
- `INSERT INTO ... VALUES` execution (single and multiple rows)
- `INSERT INTO ... SET col=val` execution
- `DELETE FROM ... WHERE ...` execution with row removal
- `DELETE FROM ... LIMIT n` support
- 11 tests covering all INSERT/DELETE paths and error cases

## Implementation
- INSERT uses `Table::add_row()` to extend the table, then `write_scalar_cell()`
  to populate the new row from evaluated expression values.
- DELETE collects matching rows via WHERE evaluation, applies LIMIT, then
  removes rows from back to front using `Table::remove_row()` to keep indices valid.
- Both commands require a writable table and flush after mutation.
