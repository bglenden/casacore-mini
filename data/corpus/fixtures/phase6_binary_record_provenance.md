# Phase 6 Binary Record Fixtures — Provenance

## Generation method

These `.bin` files were produced by `src/aipsio_fixture_gen.cpp`, which uses
`write_aipsio_record()` to serialize `Record` objects into the canonical
big-endian AipsIO format. The Record structures were manually constructed
to match the keyword layouts visible in the corresponding `showtableinfo.txt`
text fixtures.

**These are NOT casacore-produced files.** They are casacore-mini-generated
fixtures that encode the same logical metadata as the text fixtures. Their
primary purpose is testing the binary Record write/read round-trip and
cross-validating the binary metadata extraction path against the text path.

## Fixture inventory

| File | Structure |
|------|-----------|
| `logtable_stdstman_keywords/column_TIME_keywords.bin` | UNIT, MEASURE_TYPE, MEASURE_REFERENCE |
| `ms_tree/table_keywords.bin` | MS_VERSION (float), subtable references (string) |
| `ms_tree/column_UVW_keywords.bin` | QuantumUnits (string array), MEASINFO sub-record |
| `pagedimage_coords/table_keywords.bin` | Nested coords record with direction0, obsdate, maps |

## SHA256 checksums

```
fb21781ed6aed93c1622b08d62f730e7cf1ed6a19d05f7b2544b0cb50848d551  logtable_stdstman_keywords/column_TIME_keywords.bin
1284044928789459e642f70aedfe5233f2eec0ba462dc67943e0c5cbc7581d1f  ms_tree/table_keywords.bin
74083fce030a8f6ca43b04103fdc7ea8fd655d995b53e23596b78409ca8e9c45  ms_tree/column_UVW_keywords.bin
2f4a5b849d4364c09db5c02c1995069eda59d11cd03f3e121bce094ef2c9ee77  pagedimage_coords/table_keywords.bin
```

## Regeneration

```bash
cmake --build build -G Ninja
./build/aipsio_fixture_gen
```
