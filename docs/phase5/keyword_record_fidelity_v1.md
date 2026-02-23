# Keyword Record Fidelity Contract v1

Date: 2026-02-23

## Purpose

Define the scalar type and array shape fidelity requirements for
keyword/metadata record values to enable write-path parity with casacore
AipsIO-encoded table keywords.

## Background

Phase 4 introduced `KeywordValue` with compact typing (bool, int64, double,
string, array, record) suitable for text-parsed `showtableinfo` output.
The persistence-facing `RecordValue` already preserves full on-disk scalar
width fidelity and explicit multidimensional array shape.

## Contract

### Scalar type fidelity

Write-safe keyword metadata must preserve the following casacore scalar types
without lossy conversion:

| casacore type | `RecordValue` alternative | Width |
|---|---|---|
| `Bool` | `bool` | 1 byte |
| `Short` | `std::int16_t` | 2 bytes |
| `uShort` | `std::uint16_t` | 2 bytes |
| `Int` | `std::int32_t` | 4 bytes |
| `uInt` | `std::uint32_t` | 4 bytes |
| `Int64` | `std::int64_t` | 8 bytes |
| `uInt64` | `std::uint64_t` | 8 bytes |
| `Float` | `float` | 4 bytes |
| `Double` | `double` | 8 bytes |
| `Complex` | `std::complex<float>` | 8 bytes |
| `DComplex` | `std::complex<double>` | 16 bytes |
| `String` | `std::string` | variable |

### Array shape fidelity

Write-safe keyword arrays must preserve:

1. Element type at the same width as the scalar counterpart above.
2. Full multidimensional shape (`RecordArray<T>::shape`).
3. Fortran-order element layout (first axis varying fastest).

The product of shape dimensions must equal `elements.size()`.

### Implications

1. `RecordValue` (from `record.hpp`) is the write-safe value type for
   keyword metadata. It satisfies all requirements above.
2. `KeywordValue` (from `keyword_record.hpp`) remains valid for text-parsed
   intermediate representations but must not be used directly as the source
   for AipsIO write paths without conversion.
3. Future phases that implement direct binary keyword reads should produce
   `Record` values, not `KeywordRecord` values.

### AipsIO write-path encoding requirements

When writing keyword metadata via AipsIO:

- All numeric values are encoded in canonical big-endian byte order.
- Strings are encoded as `uInt` length followed by raw bytes.
- Object headers use the standard `magic | length | type_string | version`
  framing.
- Array values encode rank, per-dimension extents, and then flattened elements
  in Fortran order.

These requirements are implemented by the `AipsIoWriter` class introduced in
Phase 5.
