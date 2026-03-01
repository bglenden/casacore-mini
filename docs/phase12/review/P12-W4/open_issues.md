# P12-W4 Open Issues

- ALTER TABLE ADD COLUMN, RENAME COLUMN, DROP COLUMN, COPY COLUMN are parsed but
  execution throws "not yet supported". These require deeper table schema modification
  capabilities not yet available. They can be addressed in W10/W11 table infrastructure
  waves if needed.
- SELECT INTO and GIVING paths are parsed but not yet executed. Will be addressed
  when multi-table support is added in W5.
- DMINFO clause is parsed but not yet executed (storage manager selection at creation time).
