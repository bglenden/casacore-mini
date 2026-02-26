# Oracle Dump Format Specification

## Overview

The oracle dump is a deterministic, diff-friendly, line-based text file that
captures every structural element and cell value in a casacore MeasurementSet.
It is produced once by the upstream casacore `dump-oracle-ms` subcommand and
verified cell-by-cell by the casacore-mini `verify-oracle-ms` subcommand.

## File Structure

```
oracle_version=1
source_ms=<ms_basename>
table_count=<N>

[table sections...]
```

Tables are ordered: `MAIN` first, then subtables alphabetically by name.

## Table Section

Each table section contains, in order:

1. Table header
2. Column descriptors (in file order)
3. Table keywords (alphabetically by field path)
4. Column keywords (by column name alphabetically, then field path)
5. Cell values (by column name alphabetically, then row number)

### Table Header

```
table=<NAME>
table=<NAME>.row_count=<N>
table=<NAME>.ncol=<N>
```

`<NAME>` is `MAIN` for the main table, or the subtable name (e.g. `ANTENNA`).

### Column Descriptors

```
table=<NAME>.col[<i>].name_b64=<base64>
table=<NAME>.col[<i>].kind=scalar|array
table=<NAME>.col[<i>].dtype=TpBool|TpInt|TpFloat|TpDouble|TpComplex|TpDComplex|TpString|TpShort|TpInt64
table=<NAME>.col[<i>].ndim=<N>
table=<NAME>.col[<i>].shape=<d0,d1,...>
table=<NAME>.col[<i>].options=<int>
table=<NAME>.col[<i>].dm_type_b64=<base64>
table=<NAME>.col[<i>].dm_group_b64=<base64>
```

- `<i>` is the 0-based column index in file order.
- `shape` line is only emitted for fixed-shape columns (ndim > 0 with shape).
- `name_b64`, `dm_type_b64`, `dm_group_b64` are base64-encoded strings.

### Table Keywords

```
table=<NAME>.kw.<path>=<type>|<value>
```

Uses the same `field=<path>|<type>|<value>` encoding as `canonical_record_lines()`,
but prefixed with `table=<NAME>.kw.` and the `field=` prefix stripped.

### Column Keywords

```
table=<NAME>.col_kw[<col_b64>].<path>=<type>|<value>
```

`<col_b64>` is the base64-encoded column name.

### Cell Values

```
table=<NAME>.cell[<col_b64>][<row>]=<type>|<value>
```

## Value Encoding

| Type | Encoding | Example |
|------|----------|---------|
| Bool | `bool\|true` or `bool\|false` | `bool\|true` |
| Int16 | `int16\|<decimal>` | `int16\|-42` |
| Int32 | `int32\|<decimal>` | `int32\|100` |
| UInt32 | `uint32\|<decimal>` | `uint32\|200` |
| Int64 | `int64\|<decimal>` | `int64\|9000000000` |
| Float | `float32\|<hexfloat>` | `float32\|0x1.8p+0` |
| Double | `float64\|<hexfloat>` | `float64\|0x1.91eb851eb851fp+1` |
| Complex | `complex64\|(<real_hex>;<imag_hex>)` | `complex64\|(0x1p+0;-0x1p+1)` |
| DComplex | `complex128\|(<real_hex>;<imag_hex>)` | `complex128\|(0x1p+0;0x0p+0)` |
| String | `string\|b64:<base64>` | `string\|b64:aGVsbG8=` |

### Array Values

```
<array_type>|shape=<d0,d1,...>|values=<v0,v1,...>
```

Array element types use the same encoding as scalars (hexfloat for floats,
base64 for strings, etc.). Elements are separated by commas.

### Special Values

- Undefined cell: `undefined`
- Table reference: `table_ref\|b64:<base64_path>`

## Tolerance

The verifier uses exact hex-float comparison as the primary check. When exact
comparison fails, tolerance fallback is applied:

- `float`: absolute difference < 1e-5
- `double`: absolute difference < 1e-10
- All other types: exact match required

Tolerance matches are reported as warnings, not failures.

## Skip Allow-List

The following items are skipped during verification:

- **Subtables**: DOPPLER, SOURCE, SYSCAL (optional, not implemented)
- **Column**: FLAG_CATEGORY (declared but not populated/exercised)

Any unexpected skip is treated as a failure.
