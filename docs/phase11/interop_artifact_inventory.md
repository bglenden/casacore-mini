# Phase 11 Interop Artifact Inventory

Date: 2026-02-27

## Purpose

Defines required interop artifacts for Phase-11 matrix verification (W9/W10).

## TaQL artifact families

| ID | Description | Producer | Verifier |
|----|-------------|----------|----------|
| taql-select-basic | SELECT with WHERE, ORDER BY, LIMIT | Both toolchains | Row/value parity check |
| taql-select-join | SELECT with JOIN ON | Both toolchains | Row/value parity check |
| taql-select-groupby | SELECT with GROUP BY, HAVING, aggregates | Both toolchains | Group/aggregate parity |
| taql-update-insert-delete | UPDATE SET, INSERT VALUES, DELETE WHERE | Both toolchains | Side-effect parity |
| taql-calc-count-show | CALC expr, COUNT, SHOW | Both toolchains | Output parity |
| taql-create-alter-drop | CREATE TABLE, ALTER TABLE, DROP TABLE | Both toolchains | Schema parity |

## MSSelection artifact families

| ID | Description | Producer | Verifier |
|----|-------------|----------|----------|
| mssel-antenna-field-spw | Antenna/field/SPW selection | Both toolchains | Row+ID parity |
| mssel-time-uvdist-state | Time/UVdist/state selection | Both toolchains | Row+range parity |
| mssel-observation-feed-poln | Obs/feed/corr selection | Both toolchains | Row+ID parity |
| mssel-taql-bridge | MSSelection with TaQL clauses | Both toolchains | Combined row parity |

## Existing artifact families (full-project matrix)

These were verified in prior phases and are re-verified in W10:

| ID | Phase | Description |
|----|-------|-------------|
| record-rich | P1-P6 | Nested Record with array content |
| table-ssm | P7 | SSM table with all column types |
| table-ism | P7 | ISM table |
| table-tsm-col | P7 | TiledColumnStMan table |
| table-tsm-cell | P7 | TiledCellStMan table |
| table-tsm-shape | P7 | TiledShapeStMan table |
| table-tsm-data | P7 | TiledDataStMan table |
| measures-coords | P8 | Measure/coordinate metadata |
| ms-full | P9 | MeasurementSet with subtables |
| img-full | P10 | Image with masks/regions/expressions |

## Matrix shape (per family)

1. casacore -> casacore (self-check)
2. casacore -> casacore-mini (cross-read)
3. casacore-mini -> casacore (cross-read)
4. casacore-mini -> casacore-mini (self-check)

## Semantic pass criteria

Each cell validates:
1. Schema and keyword parity
2. Value and shape parity within documented tolerances
3. Row selection result parity (for TaQL/MSSel families)
4. Side-effect parity (for mutation commands)
