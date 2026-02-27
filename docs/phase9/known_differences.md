# Phase 9: Known Differences from Upstream casacore

## MeasurementSet Architecture

### No MSTable template hierarchy
- Upstream casacore uses `MSTable<Enum>` template class hierarchy with
  compile-time enum binding.
- casacore-mini uses schema factories plus focused wrapper classes over the
  `Table` abstraction.

## Selection Surface

### Required categories are implemented; full TaQL is not
- Phase 9 implements the required MS selection categories:
  antenna, field, spw/channel, correlation, scan, time, uvdist, state.
- Full TaQL expression parity (arbitrary expressions, regex-heavy forms,
  subqueries) is not part of this phase scope.

## Subtable Population

### Optional subtable schemas exist, writer population remains partial
- Schema factories exist for optional subtables (`DOPPLER`, `FREQ_OFFSET`,
  `SOURCE`, `SYSCAL`, `WEATHER`).
- Current `MsWriter` artifact workflows primarily populate required subtables;
  optional-subtable row population APIs are carry-forward work.

## Column/Artifact Coverage

### DATA and FLAG_CATEGORY in interop artifacts
- `DATA` (complex visibilities) is supported in the library, but Phase 9 interop
  artifacts keep payloads focused on representative required columns.
- `FLAG_CATEGORY` is declared and readable; artifact population remains minimal.

## Concatenation Semantics

### Physical concat only
- casacore-mini `ms_concat()` currently produces a physical output MS.
- Upstream casacore also supports virtual concat (`ConcatTable`).

## Flagger Behavior

### Table-level rewrite path
- casacore-mini `ms_flag_rows()` currently uses table-level read/modify/write
  behavior on underlying storage-manager-backed columns.
- Upstream casacore exposes additional in-place update paths in its broader
  table operation stack.

## Numeric Precision

### No observed precision deltas in required coverage
- `UVW`, `TIME`, and `TIME_CENTROID` round-trip at expected precision in
  current Phase 9 tests and interop gates.

## Interop Status (Current)

- Phase 9 interop matrix now passes all required cells (`20/20`).
- Oracle conformance gate passes on the upstream MSSel fixture
  (`pass=330811, fail=0`).
