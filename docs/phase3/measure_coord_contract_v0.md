# Measure/Coordinate Metadata Contract v0

Date: 2026-02-23

## Scope

This contract defines the initial normalized metadata extracted from
`showtableinfo` keyword sections for Phase 3 compatibility checks and early API
stabilization.

It is intentionally narrow and fixture-driven. It does not yet model full
casacore measure/coordinate semantics.

## Source representation

- Input: `showtableinfo` textual output.
- Parsing scope starts at the `Keywords of main table` section.

## Normalized output model

### `MeasureColumnMetadata`

- `column_name: string`
- `quantum_units: string[]`
- `measure_type: optional<string>`
- `measure_reference: optional<string>`

Population rules:

1. `UNIT: String "X"` maps to `quantum_units = ["X"]`.
2. `QuantumUnits: String array ...` maps to bracket-list values on the following
   non-empty line.
3. `MEASURE_TYPE: String "X"` maps to `measure_type = "X"`.
4. `MEASURE_REFERENCE: String "X"` maps to `measure_reference = "X"`.
5. `MEASINFO: { type: String "..."; Ref: String "..." }` maps to
   `measure_type` / `measure_reference`.

Notes:

- Case is preserved exactly as emitted by `showtableinfo`.
- Entries are emitted in first-seen order.

### `CoordinateKeywordMetadata`

- `has_coords: bool`
- `obsdate_type: optional<string>` (`coords.obsdate.type`)
- `obsdate_reference: optional<string>` (`coords.obsdate.refer`)
- `direction_system: optional<string>` (`coords.direction0.system`)
- `direction_axes: string[]` (`coords.direction0.axes`)

Population rules:

1. Presence of `coords: {` sets `has_coords = true`.
2. Nested string fields are extracted from the corresponding record paths.
3. `direction_axes` comes from the bracket-list line following
   `axes: String array ...`.

## Error and partial-data behavior

- Missing `Keywords of main table` section yields an empty result (no throw).
- Unrecognized lines are ignored.
- Malformed optional fragments are skipped; other extracted fields are retained.

## Out of scope (v0)

- Full recursive keyword-record materialization.
- Numeric quantity parsing and unit normalization.
- Coordinate transform semantics and axis mapping behaviors.
- Direct binary keyword parsing (`AipsIO`/table payload path).
