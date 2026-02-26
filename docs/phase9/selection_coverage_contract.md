# Phase 9 Selection Coverage Contract

## Required selection categories (8)

Each category must support:
1. Parse success on valid expressions
2. Parse failure with clear diagnostics on malformed expressions
3. Deterministic row-set evaluation matching casacore behavior
4. Negation support where applicable

---

## Category 1: Antenna

### Valid expressions
- `"0"` - single antenna ID
- `"0,1,2"` - antenna ID list
- `"0~3"` - antenna ID range (inclusive)
- `"0&1"` - baseline between antenna 0 and 1
- `"0&*"` - all baselines with antenna 0
- `"!5"` - exclude antenna 5
- `"ANT1"` - antenna by name (resolved via ANTENNA subtable NAME column)

### Malformed expressions
- `"0&"` - incomplete baseline
- `"abc~def"` - non-numeric range
- `"-1"` - negative antenna ID

### Evaluation semantics
Selects rows where ANTENNA1 or ANTENNA2 matches the selected antenna set.
Baseline expressions select rows where both ANTENNA1 and ANTENNA2 match.

---

## Category 2: Field

### Valid expressions
- `"0"` - field ID
- `"0,1"` - field ID list
- `"3C273"` - field by name (NAME column match)
- `"3C*"` - field by name pattern (glob)

### Malformed expressions
- `""` - empty expression

### Evaluation semantics
Selects rows where FIELD_ID matches the selected field set.

---

## Category 3: Spectral window / channel

### Valid expressions
- `"0"` - SPW ID
- `"0,1"` - SPW ID list
- `"0:0~63"` - SPW 0, channels 0-63
- `"0:0~63;128~191"` - SPW 0, channel ranges
- `"*"` - all SPWs

### Malformed expressions
- `"0:"` - missing channel specification after colon
- `"0:abc"` - non-numeric channels

### Evaluation semantics
Selects rows where DATA_DESC_ID maps to a selected SPW ID
(via DATA_DESCRIPTION subtable). Channel selection narrows the
channel axis for data access but does not eliminate rows.

---

## Category 4: Correlation / polarization

### Valid expressions
- `"XX,YY"` - correlation types by name
- `"RR,LL"` - circular correlations
- `"I,Q,U,V"` - Stokes parameters
- `"0,3"` - correlation indices

### Malformed expressions
- `"ZZ"` - unknown correlation type

### Evaluation semantics
Filters the correlation axis. Row selection is not affected;
only the correlation slice is narrowed for data access.

---

## Category 5: Scan

### Valid expressions
- `"1"` - single scan number
- `"1,3,5"` - scan number list
- `"1~5"` - scan range (inclusive)
- `"!3"` - exclude scan 3

### Malformed expressions
- `"a~b"` - non-numeric scan range

### Evaluation semantics
Selects rows where SCAN_NUMBER is in the selected set.

---

## Category 6: Time

### Valid expressions
- `">2020/01/01"` - after a date
- `"2020/01/01~2020/01/02"` - date range
- `">59000.0"` - MJD comparison
- `"59000.0~59001.0"` - MJD range

### Malformed expressions
- `"not-a-date"` - unparseable date string

### Evaluation semantics
Selects rows where TIME (in MJD seconds) falls within the specified range.
Date strings are converted to MJD seconds before comparison.

---

## Category 7: UV-distance

### Valid expressions
- `">100"` - UV distance > 100 (meters or wavelengths depending on context)
- `"100~500"` - UV distance range
- `"<50"` - UV distance less than 50

### Malformed expressions
- `"abc"` - non-numeric expression

### Evaluation semantics
Computes UV distance as sqrt(U^2 + V^2) from UVW column and selects
rows where the distance falls within the specified range.

---

## Category 8: State / observation / array

### Valid expressions
- State: `"0,1"` - state ID list
- State: `"*REFERENCE*"` - pattern match on OBS_MODE column
- Observation: `"0"` - observation ID
- Array: `"0,1"` - array ID list

### Malformed expressions
- Empty expression with no default

### Evaluation semantics
- State: selects rows where STATE_ID matches
- Observation: selects rows where OBSERVATION_ID matches
- Array: selects rows where ARRAY_ID matches

---

## Combined selection

When multiple categories are set, the final row set is the intersection
(AND) of all individual category selections.
