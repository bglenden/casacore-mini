# P8-W5 Summary

Retrospective review packet — generated 2026-02-24 during P8 closeout remediation.

## Scope implemented

- TableMeasure descriptor (`table_measure_desc.hpp/cpp`) for encoding measure
  type and reference frame metadata into table column keywords, matching the
  casacore `TableMeasDesc` convention.
- TableMeasure column (`table_measure_column.hpp/cpp`) for reading and writing
  measure values from/to table columns using keyword-based metadata.
- Tests: table_measure_desc_test, table_measure_column_test.

## Public API changes

- `table_measure_desc.hpp`: `TableMeasureDesc` class with `write_keywords()`,
  `read_keywords()`, `measure_type()`, `ref_type()`.
- `table_measure_column.hpp`: `TableMeasureColumn` class with `get_measure()`,
  `put_measure()`.

## Behavioral changes

- Table columns can now carry measure metadata in their keyword sets,
  following the casacore convention of `MEASINFO` keyword records.

## Deferred items

None.
