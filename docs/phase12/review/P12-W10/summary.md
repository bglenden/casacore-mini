# P12-W10 Summary: Table infrastructure tranche A

## What was done

1. **Lock model**: Added `TableLockMode` enum (NoLock, AutoLock, PermanentLock,
   UserLock) and lock/unlock API on Table. Lock state is persisted via
   `table.lock` file content ("read" or "write" for locked, empty for
   unlocked). `Table::lock()`, `unlock()`, `has_lock()`, `is_locked()`,
   `lock_mode()`, `set_lock_mode()` methods added.

2. **Table metadata/info surfaces**: Added `TableInfo` struct and
   `table_info()`, `table_info_type()`, `set_table_info()` methods to Table
   for reading/writing `table.info`. Added `private_keywords()` and
   `rw_private_keywords()` accessors for the table descriptor's private
   keywords. Added `has_column()` convenience method.

3. **Setup/new-table helper surfaces**: Extracted `parse_data_type_name()`
   and `data_type_to_string()` as public utility functions in `table.hpp`.
   TaQL DDL CREATE TABLE paths now use `parse_data_type_name()` instead of
   duplicated lambdas. Added `Table::drop_table()` static method with
   lock-safety check. TaQL DROP TABLE paths now use `Table::drop_table()`.

## Tests

- New `tests/table_lock_test.cpp`: 20 checks (lock/unlock, read/write lock,
  lock mode, drop_table locked/unlocked/nonexistent).
- New `tests/table_info_test.cpp`: 51 checks (table info, private keywords,
  has_column, parse_data_type_name for all types, data_type_to_string).
- All 103 tests pass (101 existing + 2 new).
