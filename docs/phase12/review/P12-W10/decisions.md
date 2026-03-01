# P12-W10 Decisions

1. Lock model uses simple file content ("read"/"write"/empty) rather than
   OS-level file locks (flock/fcntl). This is consistent with how casacore
   itself handles lock files and avoids platform-specific complexity.
2. `Table::drop_table()` checks the lock file before deletion. Force mode
   skips the check. TaQL DROP always uses force=true since the user
   explicitly issued the DDL command.
3. `parse_data_type_name()` and `data_type_to_string()` are free functions
   in the `casacore_mini` namespace (not static Table methods) since they
   are general-purpose type conversion utilities.
4. Private keywords are exposed through `table_desc.private_keywords` —
   the same path used by `TableCreateOptions::private_keywords`. No
   separate persistence needed; they are serialized with the table
   descriptor in table.dat.
5. `TableInfo` is a simple struct with `type` and `sub_type` strings,
   matching the two lines in the `table.info` text file. No version or
   endian info is included (those come from `table.dat`).
6. `has_column()` delegates to `find_column_desc()` for consistency with
   the existing column lookup pattern.
