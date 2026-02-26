# Phase 9 Operations Coverage Contract

## Required operations

### 1. MsMetaData

Cached metadata queries over a MeasurementSet.

Required behaviors:
- `antenna_names()` returns antenna names from ANTENNA subtable
- `antenna_positions()` returns position measures from ANTENNA subtable
- `antenna_diameters()` returns DISH_DIAMETER values
- `field_names()` returns field names from FIELD subtable
- `field_directions()` returns PHASE_DIR direction measures
- `spw_frequencies()` returns CHAN_FREQ arrays from SPECTRAL_WINDOW
- `spw_bandwidths()` returns TOTAL_BANDWIDTH values
- `scan_numbers()` returns unique SCAN_NUMBER values from main table
- `observation_ids()` returns unique OBSERVATION_ID values
- `n_rows()` returns main table row count
- `n_antennas()` returns ANTENNA subtable row count
- `n_fields()` returns FIELD subtable row count
- `n_spws()` returns SPECTRAL_WINDOW subtable row count

Parity requirement: all values must match casacore's MSMetaData output on
the same MS (within tolerance).

### 2. MsSummary

Text summary generation similar to casacore's `msoverview`.

Required behaviors:
- Produce summary text containing:
  - MS name and path
  - Row count
  - Antenna count and names
  - Field count and names
  - SPW count with frequency ranges
  - Scan list
  - Time range
  - Observation info

### 3. MsConcat

Concatenate two compatible MeasurementSet trees.

Required behaviors:
- Validate schema compatibility (matching column names/types)
- Remap subtable foreign keys when merging
- Merge subtable rows with deduplication by name/value
- Append main table rows with remapped IDs
- Preserve measure metadata
- Produce valid MS (passes all integrity checks)
- Error on incompatible schemas

### 4. MsFlagger

Flag manipulation on a MeasurementSet.

Required behaviors:
- Set FLAG values for specified rows
- Set FLAG_ROW for specified rows
- Clear flags for specified rows
- Query flag state for specified rows
- Flag by selection expression (delegates to MsSelection)
- Flags persist after flush and reopen

### 5. MsWriter / MSValidIds

Write flows and foreign-key validation.

Required behaviors:
- Add rows to main table with all required columns
- Add rows to subtables
- Validate foreign keys: ANTENNA1, ANTENNA2, FIELD_ID, DATA_DESC_ID,
  OBSERVATION_ID, PROCESSOR_ID, STATE_ID, FEED1, FEED2, ARRAY_ID
- Report invalid references with clear diagnostics
- Write + reread round-trip preserves all values

### 6. MsIter

Iterator over MS rows grouped by metadata.

Required behaviors:
- Group by (ARRAY_ID, FIELD_ID, DATA_DESC_ID) default grouping
- Group by time interval
- Produce sorted index sets for each group
- Iteration order is deterministic

### 7. StokesConverter

Convert between polarization types.

Required behaviors:
- I,Q,U,V <-> RR,RL,LR,LL conversion
- I,Q,U,V <-> XX,XY,YX,YY conversion
- RR,RL,LR,LL <-> XX,XY,YX,YY conversion
- Partial Stokes (e.g. I only from XX,YY) support
- Values match casacore's StokesConverter within floating-point tolerance

### 8. MsDopplerUtil

Doppler tracking utilities.

Required behaviors:
- Compute Doppler-shifted frequencies using VelocityMachine
- Support REST -> LSRK and other standard conversions

### 9. MsHistoryHandler

HISTORY subtable management.

Required behaviors:
- Add history entry with timestamp, message, origin
- Read history entries
- Entries persist after flush and reopen
