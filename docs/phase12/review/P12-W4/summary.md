# P12-W4 Summary: TaQL DDL Closure (CREATE/ALTER/DROP)

## Changes

1. **CREATE TABLE** execution via `taql_calc` (no existing table needed):
   - Parses column definitions with type aliases (BOOL, INT, INT64, SHORT, FLOAT, DOUBLE, STRING, COMPLEX, DCOMPLEX and shorthand forms)
   - Creates tables on disk using `Table::create()`
   - Also available through `taql_execute` for completeness

2. **ALTER TABLE** execution via `taql_execute`:
   - `ADDROW n`: adds rows using `Table::add_row()`
   - `SET KEYWORD key=value`: sets table keywords via `rw_keywords().set()`
   - `DROPKEY name`: removes keywords via `rw_keywords().remove()`

3. **DROP TABLE** execution via `taql_calc`:
   - Removes table directories using `std::filesystem::remove_all`
   - Silent on nonexistent paths

4. **Record::remove()** method added to support keyword removal.

5. **parse_table_name()** fix: string literals now handled correctly (unquoted via `str_val`).

6. `#include <filesystem>` added to `taql.cpp`.

## Test coverage

`taql_ddl_test.cpp` with 10 tests:
- CREATE TABLE: basic creation, type aliases, unknown type error
- ALTER TABLE: ADDROW, SET KEYWORD, DROPKEY, read-only error
- DROP TABLE: existing table, nonexistent path
- Record::remove: insertion/removal/nonexistent key

## Result

96/96 tests pass. Wave gate: 7/7 PASS.
