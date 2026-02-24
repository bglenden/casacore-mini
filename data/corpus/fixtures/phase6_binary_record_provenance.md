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
3e428725d4a1a7a92af12f98299a95a06794a9e14474a3882bd40cd96796ac72  logtable_stdstman_keywords/column_TIME_keywords.bin
dc18bd3b5b8f175de37420f24b92734cfa7d64e9c2f651560d83f7cc283fd652  ms_tree/table_keywords.bin
cfd41989431c7f1ac9fb057b92fd76d41efa1e17902ecbd2d7094e20dacf070c  ms_tree/column_UVW_keywords.bin
36976d5e50c5477c04f790b39f77e4fbd321b7778397dcb6cd45222c6300076e  pagedimage_coords/table_keywords.bin
```

## Regeneration

```bash
cmake -S . -B build -G Ninja
cmake --build build --target aipsio_fixture_gen
./build/aipsio_fixture_gen
```
