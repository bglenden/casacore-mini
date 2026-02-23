#pragma once

#include "casacore_mini/platform.hpp"
#include "casacore_mini/record.hpp"

#include <cstdint>
#include <iosfwd>
#include <span>
#include <vector>

namespace casacore_mini {

/// @file
/// @brief Deterministic binary serialization for `Record`.

/// Write a `Record` using the project-local deterministic binary encoding.
///
/// This format is internal scaffolding for current phases and is not yet
/// casacore `AipsIO` wire-compatible.
///
/// Current wire format:
/// - magic: `CCMR`
/// - format version: `1`
/// - payload: recursive tagged value tree
///
/// Encoding details:
/// - integer and floating payloads are encoded little-endian
/// - arrays encode rank, shape, and flattened Fortran-order elements
/// - nested list/record depth is bounded for safety
///
/// @throws std::runtime_error on stream write errors, unsupported/invalid value
/// state, shape/count mismatches, or depth-limit violations.
void write_record_binary(const Record& value, std::ostream& output);

/// Read one `Record` from the project-local deterministic binary encoding.
///
/// @throws std::runtime_error on invalid magic/version, malformed/truncated
/// payload, shape/count mismatches, unsupported value tags, or depth-limit
/// violations.
[[nodiscard]] Record read_record_binary(std::istream& input);

/// Serialize a record to a byte vector using `write_record_binary`.
///
/// @throws std::runtime_error with the same conditions as
/// `write_record_binary`.
[[nodiscard]] std::vector<std::uint8_t> serialize_record_binary(const Record& value);

/// Deserialize bytes produced by `serialize_record_binary`.
///
/// @throws std::runtime_error with the same conditions as
/// `read_record_binary`, and also when trailing bytes remain after one record
/// is decoded.
[[nodiscard]] Record deserialize_record_binary(std::span<const std::uint8_t> bytes);

} // namespace casacore_mini
