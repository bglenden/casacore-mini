# P10-W4 Summary: Image core models and metadata lifecycle

## Deliverables

1. **`include/casacore_mini/image.hpp`** — Full image class hierarchy:
   - `ImageInfo` struct with beam parameters, object name, type, and Record serialization
   - `ImageInterface<T>` abstract base extending `MaskedLattice<T>` with coordinates, units, metadata
   - `PagedImage<T>` disk-backed image (table with "map" column + keyword metadata)
   - `TempImage<T>` in-memory image with optional mask
   - `SubImage<T>` view into parent image with coordinate deep-copy via save/restore

2. **`src/image.cpp`** — PagedImage persistence implementation:
   - ImageInfo to_record/from_record serialization
   - PagedImage constructors (create + open) using TiledShapeStMan
   - Metadata stored as table keywords: coords, imageinfo, miscinfo, units
   - I/O specializations for float, double, complex<float>
   - Explicit template instantiations for all three types

3. **`tests/image_test.cpp`** — 74 test assertions:
   - ImageInfo roundtrip serialization (2 tests)
   - TempImage: basic, put/get, slice, put_whole, put_slice, metadata, coordinates, mask (8 tests)
   - SubImage: read, write, readonly, get_whole, put_whole, metadata forwarding, get_slice, put_slice, mask (9 tests)

## Key decisions

- CoordinateSystem is non-copyable (contains unique_ptr). SubImage uses save()/restore() for deep copy.
- TempImage takes CoordinateSystem by value (move semantics).
- PagedImage stores metadata in table keywords using Record serialization.
- PagedImage persistence tests deferred to W8 (requires full create-open-read roundtrip with flushed keywords).

## Test results

- image_test: 74 passed, 0 failed
- Full regression: 70/70 tests passed
