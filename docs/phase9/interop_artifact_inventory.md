# Phase 9 Interop Artifact Inventory

## Required artifacts (5)

Each artifact must pass all 4 matrix cells:
- casacore -> casacore
- casacore -> casacore-mini
- casacore-mini -> casacore
- casacore-mini -> casacore-mini

Total: 5 artifacts x 4 directions = 20 matrix cells.

---

## Artifact 1: ms-minimal

Minimal valid MeasurementSet with required subtables only and 0 data rows.

### Producer output
- Root directory with `table.dat`, `table.info`, `table.lock`
- All 12 required subtable directories (empty, correct schemas):
  ANTENNA, DATA_DESCRIPTION, FEED, FIELD, FLAG_CMD, HISTORY,
  OBSERVATION, POINTING, POLARIZATION, PROCESSOR, SPECTRAL_WINDOW, STATE
- Main table keywords referencing each subtable

### Semantic checks
1. `table.info` type == "Measurement Set"
2. All 12 required subtable directories exist
3. Main table keyword references point to valid subtable paths
4. Each subtable `table.dat` has correct column names and types
5. Main table row count == 0

---

## Artifact 2: ms-representative

MeasurementSet with 10+ rows of representative main-table data.

### Producer output
- Main table with 10+ rows containing:
  DATA (complex float array), FLAG (bool array), UVW (double[3]),
  WEIGHT (float[ncorr]), SIGMA (float[ncorr]), TIME (double),
  ANTENNA1/ANTENNA2 (int), FIELD_ID (int), DATA_DESC_ID (int),
  SCAN_NUMBER (int), EXPOSURE (double), INTERVAL (double),
  ARRAY_ID (int), OBSERVATION_ID (int), PROCESSOR_ID (int),
  STATE_ID (int), FEED1/FEED2 (int), FLAG_ROW (bool)
- Populated subtables: ANTENNA (2+ rows), SPECTRAL_WINDOW (1+),
  POLARIZATION (1+), DATA_DESCRIPTION (1+), FIELD (1+),
  OBSERVATION (1+), PROCESSOR (1+), STATE (1+), FEED (2+)
- Measure metadata: TIME has MEpoch MEASINFO, UVW has Muvw MEASINFO

### Semantic checks
1. All main-table required columns present with correct types
2. Row count >= 10
3. Cell values round-trip exactly (within tolerance)
4. Measure metadata (MEASINFO) on TIME and UVW columns
5. Foreign keys (ANTENNA1/2, FIELD_ID, DATA_DESC_ID, etc.) reference valid subtable rows
6. DATA shape matches (ncorr, nchan) from POLARIZATION + SPECTRAL_WINDOW

---

## Artifact 3: ms-optional-subtables

MeasurementSet including optional subtable population.

### Producer output
- Same as ms-representative, plus populated optional subtables:
  WEATHER (1+ row), SOURCE (1+ row), POINTING (1+ row),
  SYSCAL (1+ row), FREQ_OFFSET (1+ row), DOPPLER (1+ row)

### Semantic checks
1. All required subtable checks from ms-representative pass
2. Each optional subtable present with correct schema
3. Optional subtable row counts >= 1
4. Column values in optional subtables round-trip correctly
5. Main table keywords reference all present subtables

---

## Artifact 4: ms-concat

Two compatible MeasurementSet inputs plus concatenated output.

### Producer output
- Two input MS directories (ms-concat-a, ms-concat-b) with overlapping
  antenna/field/spw definitions and distinct time ranges
- One concatenated output MS directory (ms-concat-out)

### Semantic checks
1. Output row count == input_a.rows + input_b.rows
2. Subtable IDs correctly remapped (no orphan references)
3. Antenna/field/spw subtables merged (deduplicated by name/value)
4. All foreign keys in output reference valid subtable rows
5. Cell values from both inputs present in output (with tolerance)
6. Time ordering preserved within each input's contribution

---

## Artifact 5: ms-selection-stress

Multi-field, multi-spw, multi-scan, multi-array MeasurementSet for selection testing.

### Producer output
- 50+ rows spanning:
  - 3+ fields (FIELD_ID 0,1,2)
  - 2+ spectral windows (DATA_DESC_ID 0,1)
  - 3+ scans (SCAN_NUMBER 1,2,3)
  - 2+ arrays (ARRAY_ID 0,1)
  - 4+ antennas (ANTENNA1/2 from {0,1,2,3})
  - 2+ states (STATE_ID 0,1)
  - Time range spanning > 60 seconds
  - UVW values with varying magnitudes for uvdist selection

### Semantic checks
1. All main-table and subtable schema checks pass
2. Correct field/spw/scan/array/state distributions
3. Selection expressions produce correct row subsets:
   - antenna "0,1" selects only rows with ANTENNA1 or ANTENNA2 in {0,1}
   - field "0" selects only rows with FIELD_ID==0
   - spw "0" selects only rows with DATA_DESC_ID==0
   - scan "1~2" selects scans 1 and 2
   - time range selects expected subset
   - uvdist range selects expected subset
4. Combined selection (AND logic) produces intersection of individual selections
