#pragma once

#include "casacore_mini/aipsio_reader.hpp"
#include "casacore_mini/record.hpp"

namespace casacore_mini {

/// @file
/// @brief Decode casacore `Record` objects from canonical `AipsIO` byte streams.

/// Read a casacore-encoded `Record` from an `AipsIO` byte stream.
///
/// This decodes the binary format produced by casacore `RecordRep::putRecord`
/// including the RecordDesc, field values, nested sub-records, and arrays.
///
/// Supported object types: `Record`, `RecordDesc`, `Array`, `IPosition`.
///
/// @note `Char` and `uChar` scalars and arrays are losslessly promoted to
/// `int16_t` and `uint16_t` respectively since `RecordValue` has no 8-bit type.
///
/// @throws std::runtime_error on malformed input, unsupported object type,
/// truncated data, negative size fields, or nesting depth limit exceeded.
[[nodiscard]] Record read_aipsio_record(AipsIoReader& reader);

} // namespace casacore_mini
