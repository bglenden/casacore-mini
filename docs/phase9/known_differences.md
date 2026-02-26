# Phase 9: Known Differences from Upstream casacore

## MeasurementSet Architecture

### No MSTable template hierarchy
- Upstream casacore uses `MSTable<Enum>` template class hierarchy with
  compile-time column enum binding. casacore-mini uses schema definition
  functions that produce `TableDesc`/`TableDatFull` structs, with thin
  wrapper classes composing over SSM/TSM readers/writers.
- This is a deliberate design choice per the Phase 9 plan; the external
  API surface is equivalent.

### No TaQL-based selection
- Upstream casacore uses TaQL (Table Query Language) for complex selection
  expressions. casacore-mini uses a simplified expression evaluator with
  casacore-compatible syntax for the 8 required categories (antenna, field,
  spw, scan, time, uvdist, correlation, state).
- Supported syntax covers the most common selection patterns. Advanced TaQL
  features (arbitrary expressions, regex, subquery) are not supported.

## Subtable Coverage

### Required subtables (12): fully implemented
- ANTENNA, DATA_DESCRIPTION, FEED, FIELD, FLAG_CMD, HISTORY, OBSERVATION,
  POINTING, POLARIZATION, PROCESSOR, SPECTRAL_WINDOW, STATE.

### Optional subtables (5): not implemented
- DOPPLER, FREQ_OFFSET, SOURCE, SYSCAL, WEATHER are not implemented.
- casacore-mini creates only the 12 required subtables. Opening an MS with
  optional subtables works (they are ignored), but writing optional subtable
  data is not supported.

## Column Coverage

### DATA column
- The DATA column (complex visibility array) is supported via TiledStMan.
  However, interop artifacts do not include DATA columns to keep the
  artifacts simple and portable.

### FLAG_CATEGORY
- FLAG_CATEGORY (3D flag array) is declared in the schema but not populated
  in interop artifacts or exercised in tests.

## Selection Behavior

### Antenna expression
- Supports: ID list, range (`~`), baseline (`&`), negation (`!`), name,
  glob pattern (`*`, `?`).
- Not supported: regex patterns, antenna-pair lists with mixed names/IDs
  in the same expression.

### Time expression
- Supports: `>value`, `<value`, `lo~hi` range in MJD seconds.
- Not supported: calendar date strings (e.g., `2020/01/01`). Upstream
  casacore accepts both MJD and calendar formats.

### UV-distance expression
- Supports: `>value`, `<value`, `lo~hi` range in meters.
- Not supported: wavelength units. Upstream casacore supports both meters
  and wavelengths.

## Concatenation

### Subtable deduplication
- ANTENNA, FIELD, SPECTRAL_WINDOW rows are deduplicated by NAME.
- DATA_DESCRIPTION rows are deduplicated by (SPW_ID, POL_ID) pair.
- STATE rows are always appended (no deduplication).
- Upstream casacore's MSConcat has more sophisticated merging logic,
  including time-range merging for OBSERVATION rows.

### No virtual concatenation
- casacore-mini always produces a physical concatenated MS. Upstream
  casacore supports virtual concatenation (ConcatTable) where the output
  is a lightweight reference to the input MSes.

## Flagger

### Read-modify-write pattern
- casacore-mini's `ms_flag_rows()` uses a read-modify-write pattern on
  the SSM data file. It reads all scalar columns, modifies FLAG_ROW, and
  rewrites the entire SSM.
- Upstream casacore's MSFlagger can modify FLAG_ROW in-place via Table's
  column-level putCell operations.

## Numeric Precision

### UVW values
- UVW coordinates are stored as float64 and round-trip exactly through
  SSM storage. No precision differences from upstream casacore.

### Time values
- TIME and TIME_CENTROID are stored as float64 MJD seconds and round-trip
  exactly. No precision differences from upstream casacore.

## Interop Artifacts

- Mini-to-mini round-trip passes for all 5 artifacts.
- Cross-tool (mini↔casacore) verification checks structural properties
  (row counts, subtable counts, scan/array_id sets) rather than exact
  cell values, accommodating implementation differences in default values
  and column ordering.
