# P12-W4 Decisions

1. CREATE TABLE and DROP TABLE are routed through `taql_calc` since they don't require
   an existing table context. They also remain available through `taql_execute`.
2. ALTER TABLE column mutations (ADD/DROP/RENAME/COPY COLUMN) are deferred to the table
   infrastructure waves (W10/W11) since they require schema modification APIs not yet
   available.
3. Record::remove() was added as a new method rather than using erase-by-key on the
   entries vector directly, keeping the Record API clean.
4. parse_table_name() was fixed to handle string literals (single/double-quoted paths)
   using the token's str_val instead of raw text.
